// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/crw_browsing_data_store.h"

#import <Foundation/Foundation.h>

#include "base/ios/ios_util.h"
#import "base/ios/weak_nsobject.h"
#include "base/logging.h"
#import "base/mac/scoped_nsobject.h"
#import "ios/web/browsing_data_managers/crw_cookie_browsing_data_manager.h"
#include "ios/web/public/active_state_manager.h"
#include "ios/web/public/browser_state.h"

namespace {
// Represents the type of operations that a CRWBrowsingDataStore can perform.
enum OperationType {
  // Represents NOP.
  NONE = 1,
  // Stash operation involves stashing browsing data that web views create to
  // the associated BrowserState's state path.
  STASH,
  // Restore operation involves restoring browsing data from the
  // associated BrowserState's state path so that web views (UIWebViews and
  // WKWebViews) can read from them.
  RESTORE,
  // Remove operation involves removing the data that web views create.
  REMOVE,
};

// The name of the NSOperation that performs a restore operation.
NSString* const kRestoreOperationName = @"CRWBrowsingDataStore.RESTORE";
// The name of the NSOperation that performs a stash operation.
NSString* const kStashOperationName = @"CRWBrowsingDataStore.STASH";
}  // namespace

@interface CRWBrowsingDataStore ()
// Returns a serial queue on which stash and restore operations can be scheduled
// to be run. All stash/restore operations need to be run on the same queue
// hence it is shared with all CRWBrowsingDataStores.
+ (NSOperationQueue*)operationQueueForStashAndRestoreOperations;
// Returns a concurrent queue on which remove operations can be scheduled to be
// run. All remove operations need to be run on the same queue hence it is
// shared with all CRWBrowsingDataStores.
+ (NSOperationQueue*)operationQueueForRemoveOperations;
- (instancetype)init NS_UNAVAILABLE;

// The array of all browsing data managers. Must be accessed from the main
// thread.
@property(nonatomic, readonly) NSArray* allBrowsingDataManagers;
// Returns an array of browsing data managers for the given |browsingDataTypes|.
- (NSArray*)browsingDataManagersForBrowsingDataTypes:
        (web::BrowsingDataTypes)browsingDataTypes;
// Returns the selector that needs to be performed on the
// CRWBrowsingDataManagers for the |operationType|. |operationType| cannot be
// |NONE|.
- (SEL)browsingDataManagerSelectorForOperationType:(OperationType)operationType;
// Returns the selector that needs to be performed on the
// CRWBrowsingDataManagers for a REMOVE operation.
- (SEL)browsingDataManagerSelectorForRemoveOperationType;

// Redefined to be read-write. Must be called from the main thread.
@property(nonatomic, assign) web::BrowsingDataStoreMode mode;
// Sets the mode iff there are no more stash or restore operations that are
// still pending. |mode| can only be either |ACTIVE| or |INACTIVE|.
// |handler| is called immediately (in the same runloop) with a BOOL indicating
// whether the mode change was successful or not. |handler| can be nil.
- (void)finalizeChangeToMode:(web::BrowsingDataStoreMode)mode
    andCallCompletionHandler:(void (^)(BOOL modeChangeWasSuccessful))handler;

// Changes the mode of the CRWBrowsingDataStore to |mode|. This is an
// asynchronous operation and the mode is not changed immediately.
// |completionHandler| can be nil.
// |completionHandler| is called on the main thread. This block has no return
// value and takes a single BOOL argument that indicates whether or not the
// mode change was successfully changed to |mode|.
- (void)changeMode:(web::BrowsingDataStoreMode)mode
    completionHandler:(void (^)(BOOL modeChangeWasSuccessful))completionHandler;

// The number of stash or restore operations that are still pending.
@property(nonatomic, assign) NSUInteger numberOfPendingStashOrRestoreOperations;

// Performs operations of type |operationType| on each of the
// |browsingDataManagers|. |operationType| cannot be |NONE|.
// Precondition: There must be no web views associated with the BrowserState.
// |completionHandler| is called on the main thread and cannot be nil.
- (void)performOperationWithType:(OperationType)operationType
            browsingDataManagers:(NSArray*)browsingDataManagers
               completionHandler:(ProceduralBlock)completionHandler;

// Creates an NSOperation that calls |selector| on all the
// |browsingDataManagers|. |selector| needs to be one of the methods in
// CRWBrowsingDataManager. The created NSOperation will have a completionBlock
// of |completionHandler|.
- (NSOperation*)
    newOperationWithBrowsingDataManagers:(NSArray*)browsingDataManagers
                                selector:(SEL)selector
                       completionHandler:(ProceduralBlock)completionHandler;
// Enqueues |operation| to be run on |queue|. All operations are serialized to
// be run one after another.
- (void)addOperation:(NSOperation*)operation toQueue:(NSOperationQueue*)queue;
@end

@implementation CRWBrowsingDataStore {
  web::BrowserState* _browserState;  // Weak, owns this object.
  // The delegate.
  base::WeakNSProtocol<id<CRWBrowsingDataStoreDelegate>> _delegate;
  // The mode of the CRWBrowsingDataStore.
  web::BrowsingDataStoreMode _mode;
  // The dictionary that maps a browsing data type to its
  // CRWBrowsingDataManager.
  base::scoped_nsobject<NSDictionary> _browsingDataTypeMap;
  // The last operation that was enqueued to be run. Can be stash, restore or a
  // delete operation.
  base::scoped_nsobject<NSOperation> _lastDispatchedOperation;
  // The last dispatched stash or restore operation that was enqueued to be run.
  base::scoped_nsobject<NSOperation> _lastDispatchedStashOrRestoreOperation;
  // The number of stash or restore operations that are still pending. If this
  // value > 0 the mode of the CRWBrowsingDataStore is |CHANGING|. The mode
  // can be made ACTIVE or INACTIVE only be set when this value is 0.
  NSUInteger _numberOfPendingStashOrRestoreOperations;
}

+ (NSOperationQueue*)operationQueueForStashAndRestoreOperations {
  static dispatch_once_t onceToken = 0;
  static NSOperationQueue* operationQueueForStashAndRestoreOperations = nil;
  dispatch_once(&onceToken, ^{
    operationQueueForStashAndRestoreOperations =
        [[NSOperationQueue alloc] init];
    [operationQueueForStashAndRestoreOperations
        setMaxConcurrentOperationCount:1U];
    if (base::ios::IsRunningOnIOS8OrLater()) {
      [operationQueueForStashAndRestoreOperations
          setQualityOfService:NSQualityOfServiceUserInteractive];
    }
  });
  return operationQueueForStashAndRestoreOperations;
}

+ (NSOperationQueue*)operationQueueForRemoveOperations {
  static dispatch_once_t onceToken = 0;
  static NSOperationQueue* operationQueueForRemoveOperations = nil;
  dispatch_once(&onceToken, ^{
    operationQueueForRemoveOperations = [[NSOperationQueue alloc] init];
    [operationQueueForRemoveOperations
        setMaxConcurrentOperationCount:NSUIntegerMax];
    if (base::ios::IsRunningOnIOS8OrLater()) {
      [operationQueueForRemoveOperations
          setQualityOfService:NSQualityOfServiceUserInitiated];
    }
  });
  return operationQueueForRemoveOperations;
}

- (instancetype)initWithBrowserState:(web::BrowserState*)browserState {
  self = [super init];
  if (self) {
    DCHECK([NSThread isMainThread]);
    // TODO(shreyasv): Instantiate the necessary CRWBrowsingDataManagers that
    // are encapsulated within this class. crbug.com/480654.
    _browserState = browserState;
    web::ActiveStateManager* activeStateManager =
        web::BrowserState::GetActiveStateManager(browserState);
    DCHECK(activeStateManager);
    _mode = activeStateManager->IsActive() ? web::ACTIVE : web::INACTIVE;
    // TODO(shreyasv): If the creation of CRWBrowsingDataManagers turns out to
    // be an expensive operations re-visit this with a lazy-evaluation approach.
    base::scoped_nsobject<CRWCookieBrowsingDataManager>
        cookieBrowsingDataManager([[CRWCookieBrowsingDataManager alloc]
            initWithBrowserState:browserState]);
    _browsingDataTypeMap.reset([@{
      @(web::BROWSING_DATA_TYPE_COOKIES) : cookieBrowsingDataManager,
    } retain]);
  }
  return self;
}

- (instancetype)init {
  NOTREACHED();
  return nil;
}

- (NSString*)description {
  NSString* format = @"<%@: %p; hasPendingOperations = { %@ }>";
  NSString* hasPendingOperationsString =
      [self hasPendingOperations] ? @"YES" : @"NO";
  NSString* result =
      [NSString stringWithFormat:format, NSStringFromClass(self.class), self,
                                 hasPendingOperationsString];
  return result;
}


- (NSArray*)allBrowsingDataManagers {
  DCHECK([NSThread isMainThread]);
  return [_browsingDataTypeMap allValues];
}

- (NSArray*)browsingDataManagersForBrowsingDataTypes:
        (web::BrowsingDataTypes)browsingDataTypes {
  __block NSMutableArray* result = [NSMutableArray array];
  [_browsingDataTypeMap
      enumerateKeysAndObjectsUsingBlock:^(NSNumber* dataType,
                                          id<CRWBrowsingDataManager> manager,
                                          BOOL*) {
        if ([dataType unsignedIntegerValue] & browsingDataTypes) {
          [result addObject:manager];
        }
      }];
  return result;
}

- (id<CRWBrowsingDataStoreDelegate>)delegate {
  return _delegate;
}

- (void)setDelegate:(id<CRWBrowsingDataStoreDelegate>)delegate {
  _delegate.reset(delegate);
}

- (SEL)browsingDataManagerSelectorForOperationType:
        (OperationType)operationType {
  switch (operationType) {
    case NONE:
      NOTREACHED();
      return nullptr;
    case STASH:
      return @selector(stashData);
    case RESTORE:
      return @selector(restoreData);
    case REMOVE:
      return [self browsingDataManagerSelectorForRemoveOperationType];
  };
  NOTREACHED();
  return nullptr;
}

- (SEL)browsingDataManagerSelectorForRemoveOperationType {
  if (self.mode == web::ACTIVE) {
    return @selector(removeDataAtCanonicalPath);
  }
  if (self.mode == web::INACTIVE) {
    return @selector(removeDataAtStashPath);
  }
  DCHECK(_lastDispatchedStashOrRestoreOperation);
  NSString* lastDispatchedStashOrRestoreOperationName =
      [_lastDispatchedStashOrRestoreOperation name];
  if ([lastDispatchedStashOrRestoreOperationName
          isEqual:kRestoreOperationName]) {
    return @selector(removeDataAtCanonicalPath);
  }
  if ([lastDispatchedStashOrRestoreOperationName isEqual:kStashOperationName]) {
    return @selector(removeDataAtStashPath);
  }
  NOTREACHED();
  return nullptr;
}

+ (BOOL)automaticallyNotifiesObserversForKey:(NSString*)key {
  // It is necessary to override this for |mode| because the default KVO
  // behavior in NSObject is to fire a notification irrespective of if an actual
  // change was made to the ivar or not. The |mode| property needs fine grained
  // control over the actual notifications being fired since observers need to
  // be notified iff the |mode| actually changed.
  if ([key isEqual:@"mode"]) {
    return NO;
  }
  return [super automaticallyNotifiesObserversForKey:(NSString*)key];
}

- (web::BrowsingDataStoreMode)mode {
  DCHECK([NSThread isMainThread]);
  return _mode;
}

- (void)setMode:(web::BrowsingDataStoreMode)mode {
  DCHECK([NSThread isMainThread]);
  if (_mode == mode) {
    return;
  }
  if (mode == web::ACTIVE || mode == web::INACTIVE) {
    DCHECK(!self.numberOfPendingStashOrRestoreOperations);
  }
  [self willChangeValueForKey:@"mode"];
  _mode = mode;
  [self didChangeValueForKey:@"mode"];
}

- (void)finalizeChangeToMode:(web::BrowsingDataStoreMode)mode
    andCallCompletionHandler:(void (^)(BOOL modeChangeWasSuccessful))handler {
  DCHECK([NSThread isMainThread]);
  DCHECK_NE(web::CHANGING, mode);

  BOOL modeChangeWasSuccessful = NO;
  if (!self.numberOfPendingStashOrRestoreOperations) {
    [self setMode:mode];
    modeChangeWasSuccessful = YES;
  }
  if (handler) {
    handler(modeChangeWasSuccessful);
  }
}

- (NSUInteger)numberOfPendingStashOrRestoreOperations {
  DCHECK([NSThread isMainThread]);
  return _numberOfPendingStashOrRestoreOperations;
}

- (void)setNumberOfPendingStashOrRestoreOperations:
        (NSUInteger)numberOfPendingStashOrRestoreOperations {
  DCHECK([NSThread isMainThread]);
  _numberOfPendingStashOrRestoreOperations =
      numberOfPendingStashOrRestoreOperations;
}

- (void)makeActiveWithCompletionHandler:
        (void (^)(BOOL success))completionHandler {
  DCHECK([NSThread isMainThread]);

  [self changeMode:web::ACTIVE completionHandler:completionHandler];
}

- (void)makeInactiveWithCompletionHandler:
        (void (^)(BOOL success))completionHandler {
  DCHECK([NSThread isMainThread]);

  [self changeMode:web::INACTIVE completionHandler:completionHandler];
}

- (void)changeMode:(web::BrowsingDataStoreMode)mode
    completionHandler:
        (void (^)(BOOL modeChangeWasSuccessful))completionHandler {
  DCHECK([NSThread isMainThread]);

  ProceduralBlock completionHandlerAfterPerformingOperation = ^{
    [self finalizeChangeToMode:mode andCallCompletionHandler:completionHandler];
  };

  // Already in the desired mode.
  if (self.mode == mode) {
    // As a caller of this API, it is awkward to get the callback before the
    // method call has completed, hence defer it.
    dispatch_async(dispatch_get_main_queue(), ^{
      completionHandlerAfterPerformingOperation();
    });
    return;
  }

  OperationType operationType = NONE;
  if (mode == web::ACTIVE) {
    // By default a |RESTORE| operation is performed when the mode is changed
    // to |ACTIVE|.
    operationType = RESTORE;
    web::BrowsingDataStoreMakeActivePolicy makeActivePolicy =
        [_delegate decideMakeActiveOperationPolicyForBrowsingDataStore:self];
    operationType = (makeActivePolicy == web::ADOPT) ? REMOVE : RESTORE;
  } else {
    // By default a |STASH| operation is performed when the mode is changed to
    // |INACTIVE|.
    operationType = STASH;
    web::BrowsingDataStoreMakeInactivePolicy makeInactivePolicy =
        [_delegate decideMakeInactiveOperationPolicyForBrowsingDataStore:self];
    operationType = (makeInactivePolicy == web::DELETE) ? REMOVE : STASH;
  }
  DCHECK_NE(NONE, operationType);
  [self performOperationWithType:operationType
            browsingDataManagers:[self allBrowsingDataManagers]
               completionHandler:completionHandlerAfterPerformingOperation];
}

- (void)removeDataOfTypes:(web::BrowsingDataTypes)browsingDataTypes
        completionHandler:(ProceduralBlock)completionHandler {
  DCHECK([NSThread isMainThread]);

  NSArray* browsingDataManagers =
      [self browsingDataManagersForBrowsingDataTypes:browsingDataTypes];
  [self performOperationWithType:REMOVE
            browsingDataManagers:browsingDataManagers
               completionHandler:^{
                 // Since this may be called on a background thread, bounce to
                 // the main thread.
                 dispatch_async(dispatch_get_main_queue(), completionHandler);
               }];
}

- (BOOL)hasPendingOperations {
  if (!_lastDispatchedOperation) {
    return NO;
  }
  return ![_lastDispatchedOperation isFinished];
}

- (void)performOperationWithType:(OperationType)operationType
            browsingDataManagers:(NSArray*)browsingDataManagers
               completionHandler:(ProceduralBlock)completionHandler {
  DCHECK([NSThread isMainThread]);
  DCHECK(completionHandler);
  DCHECK_NE(NONE, operationType);

  SEL selector =
      [self browsingDataManagerSelectorForOperationType:operationType];
  DCHECK(selector);

  if (operationType == RESTORE || operationType == STASH) {
    [self setMode:web::CHANGING];
    ++self.numberOfPendingStashOrRestoreOperations;
    completionHandler = ^{
      --self.numberOfPendingStashOrRestoreOperations;
      // It is safe to this and does not lead to the block (|completionHandler|)
      // retaining itself.
      completionHandler();
    };
  }

  id callCompletionHandlerOnMainThread = ^{
    // This is called on a background thread, hence the need to bounce to the
    // main thread.
    dispatch_async(dispatch_get_main_queue(), ^{
      completionHandler();
    });
  };
  base::scoped_nsobject<NSOperation> operation([self
      newOperationWithBrowsingDataManagers:browsingDataManagers
                                  selector:selector
                         completionHandler:callCompletionHandlerOnMainThread]);

  if (operationType == RESTORE || operationType == STASH) {
    [operation setName:(RESTORE ? kRestoreOperationName : kStashOperationName)];
    _lastDispatchedStashOrRestoreOperation.reset([operation retain]);
  }

  NSOperationQueue* queue = nil;
  switch (operationType) {
    case STASH:
    case RESTORE:
      queue = [CRWBrowsingDataStore operationQueueForStashAndRestoreOperations];
      break;
    case REMOVE:
      queue = [CRWBrowsingDataStore operationQueueForRemoveOperations];
      break;
    default:
      NOTREACHED();
      break;
  }

  [self addOperation:operation toQueue:queue];
}

- (NSOperation*)
    newOperationWithBrowsingDataManagers:(NSArray*)browsingDataManagers
                                selector:(SEL)selector
                       completionHandler:(ProceduralBlock)completionHandler {
  NSBlockOperation* operation = [[NSBlockOperation alloc] init];
  for (id<CRWBrowsingDataManager> manager : browsingDataManagers) {
    // |addExecutionBlock| farms out the different blocks added to it. hence the
    // operations are implicitly parallelized.
    [operation addExecutionBlock:^{
      [manager performSelector:selector];
    }];
  }
  [operation setCompletionBlock:completionHandler];
  return operation;
}

- (void)addOperation:(NSOperation*)operation toQueue:(NSOperationQueue*)queue {
  DCHECK([NSThread isMainThread]);
  DCHECK(operation);
  DCHECK(queue);

  if (_lastDispatchedOperation) {
    [operation addDependency:_lastDispatchedOperation];
  }
  _lastDispatchedOperation.reset([operation retain]);
  [queue addOperation:operation];
}

@end
