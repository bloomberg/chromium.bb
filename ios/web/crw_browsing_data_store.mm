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
  // associated BrowserState's state path so that web views can read from them.
  RESTORE,
  // Remove operation involves removing the data that web views create.
  REMOVE,
};

// The name of the NSOperation that performs a |RESTORE| operation.
NSString* const kRestoreOperationName = @"CRWBrowsingDataStore.RESTORE";
// The name of the NSOperation that performs a |STASH| operation.
NSString* const kStashOperationName = @"CRWBrowsingDataStore.STASH";
}  // namespace

// CRWBrowsingDataStore is implemented using 2 queues.
// 1) operationQueueForStashAndRestoreOperations:
// - This queue is used to perform |STASH| and |RESTORE| operations.
// - |STASH|, |RESTORE| operations are operations that have been explicitly
//   requested by the user. And the performance of these operations block
//   further user interaction. Hence this queue has a QoS of
//   NSQualityOfServiceUserInitiated.
// - |STASH|, |RESTORE| operations from 2 different CRWBrowsingDataStores are
//    not re-entrant. Hence this is a serial queue.
// 2) operationQueueForRemoveOperations:
// - This queue is used to perform |REMOVE| operations.
// - |REMOVE| operations is an operation that has been explicitly requested by
//   the user. And the performance of this operation blocks further user
//   interaction. Hence this queue has a QoS of NSQualityOfServiceUserInitiated.
// - |REMOVE| operations from 2 different CRWBrowingDataStores can be run in
//   parallel. Hence this is a concurrent queue.
//
// |STASH|, |RESTORE|, |REMOVE| operations of a particular CRWBrowsingDataStore
// are not re-entrant. Hence these operations are serialized.

@interface CRWBrowsingDataStore ()
// Redefined to be read-write. Must be called from the main thread.
@property(nonatomic, assign) web::BrowsingDataStoreMode mode;
// The array of all browsing data managers. Must be called from the main
// thread.
@property(nonatomic, readonly) NSArray* allBrowsingDataManagers;
// The number of |STASH| or |RESTORE| operations that are still pending. Must be
// called from the main thread.
@property(nonatomic, assign) NSUInteger numberOfPendingStashOrRestoreOperations;

// Returns a serial queue on which |STASH| and |RESTORE| operations can be
// scheduled to be run. All |STASH|/|RESTORE| operations need to be run on the
// same queue hence it is shared with all CRWBrowsingDataStores.
+ (NSOperationQueue*)operationQueueForStashAndRestoreOperations;
// Returns a concurrent queue on which remove operations can be scheduled to be
// run. All |REMOVE| operations need to be run on the same queue hence it is
// shared with all CRWBrowsingDataStores.
+ (NSOperationQueue*)operationQueueForRemoveOperations;

// Returns an array of CRWBrowsingDataManagers for the given
// |browsingDataTypes|.
- (NSArray*)browsingDataManagersForBrowsingDataTypes:
    (web::BrowsingDataTypes)browsingDataTypes;
// Returns the selector that needs to be performed on the
// CRWBrowsingDataManagers for the |operationType|. |operationType| cannot be
// |NONE|.
- (SEL)browsingDataManagerSelectorForOperationType:(OperationType)operationType;
// Returns the selector that needs to be performed on the
// CRWBrowsingDataManagers for a |REMOVE| operation.
- (SEL)browsingDataManagerSelectorForRemoveOperationType;

// Sets the mode iff there are no more |STASH| or |RESTORE| operations that are
// still pending. |mode| can only be either |ACTIVE| or |INACTIVE|.
// Returns YES if the mode was successfully changed to |mode|.
- (BOOL)finalizeChangeToMode:(web::BrowsingDataStoreMode)mode;
// Changes the mode of the CRWBrowsingDataStore to |mode|. This is an
// asynchronous operation and the mode is not changed immediately.
// |completionHandler| can be nil.
// |completionHandler| is called on the main thread. This block has no return
// value and takes a single BOOL argument that indicates whether or not the
// mode change was successfully changed to |mode|.
- (void)changeMode:(web::BrowsingDataStoreMode)mode
    completionHandler:(void (^)(BOOL modeChangeWasSuccessful))completionHandler;

// Returns the OperationType (that needs to be performed) in order to change the
// mode to |mode|. Consults the delegate if one is present. |mode| cannot be
// |CHANGING|.
- (OperationType)operationTypeToChangeMode:(web::BrowsingDataStoreMode)mode;
// Performs an operation of type |operationType| on each of the
// |browsingDataManagers|. |operationType| cannot be |NONE|.
// Precondition: There must be no web views associated with the BrowserState.
// |completionHandler| is called on the main thread and cannot be nil.
- (void)performOperationWithType:(OperationType)operationType
            browsingDataManagers:(NSArray*)browsingDataManagers
               completionHandler:(ProceduralBlock)completionHandler;
// Returns an NSOperation that calls |selector| on all the
// |browsingDataManagers|. |selector| needs to be one of the methods in
// CRWBrowsingDataManager.
- (NSOperation*)operationWithBrowsingDataManagers:(NSArray*)browsingDataManagers
                                         selector:(SEL)selector;
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
  // The last operation that was enqueued to be run. Can be a |STASH|, |RESTORE|
  // or a |REMOVE| operation.
  base::scoped_nsobject<NSOperation> _lastDispatchedOperation;
  // The last dispatched |STASH| or |RESTORE| operation that was enqueued to be
  // run.
  base::scoped_nsobject<NSOperation> _lastDispatchedStashOrRestoreOperation;
  // The number of |STASH| or |RESTORE| operations that are still pending. If
  // the number of |STASH| or |RESTORE| operations is greater than 0U, the mode
  // of the CRWBrowsingDataStore is |CHANGING|. The mode can be made ACTIVE or
  // INACTIVE only be set when this value is 0U.
  NSUInteger _numberOfPendingStashOrRestoreOperations;
}

#pragma mark -
#pragma mark NSObject Methods

- (instancetype)initWithBrowserState:(web::BrowserState*)browserState {
  self = [super init];
  if (self) {
    DCHECK([NSThread isMainThread]);
    DCHECK(browserState);
    _browserState = browserState;
    web::ActiveStateManager* activeStateManager =
        web::BrowserState::GetActiveStateManager(browserState);
    DCHECK(activeStateManager);
    _mode = activeStateManager->IsActive() ? web::ACTIVE : web::INACTIVE;
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
  NSString* format = @"<%@: %p; hasPendingOperations = { %@ }; mode = { %@ }>";
  NSString* hasPendingOperationsString =
      [self hasPendingOperations] ? @"YES" : @"NO";
  NSString* modeString = nil;
  switch (self.mode) {
    case web::ACTIVE:
      modeString = @"ACTIVE";
      break;
    case web::CHANGING:
      modeString = @"CHANGING";
      break;
    case web::INACTIVE:
      modeString = @"INACTIVE";
      break;
  }
  NSString* result = [NSString stringWithFormat:format,
      NSStringFromClass(self.class), self, hasPendingOperationsString,
      modeString];
  return result;
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
  return [super automaticallyNotifiesObserversForKey:key];
}

#pragma mark -
#pragma mark Public Properties

- (id<CRWBrowsingDataStoreDelegate>)delegate {
  return _delegate;
}

- (void)setDelegate:(id<CRWBrowsingDataStoreDelegate>)delegate {
  _delegate.reset(delegate);
}

- (web::BrowsingDataStoreMode)mode {
  DCHECK([NSThread isMainThread]);
  return _mode;
}

- (BOOL)hasPendingOperations {
  if (!_lastDispatchedOperation) {
    return NO;
  }
  return ![_lastDispatchedOperation isFinished];
}

#pragma mark -
#pragma mark Public Methods

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

- (void)removeDataOfTypes:(web::BrowsingDataTypes)browsingDataTypes
        completionHandler:(ProceduralBlock)completionHandler {
  DCHECK([NSThread isMainThread]);

  NSArray* browsingDataManagers =
      [self browsingDataManagersForBrowsingDataTypes:browsingDataTypes];
  [self performOperationWithType:REMOVE
            browsingDataManagers:browsingDataManagers
               completionHandler:completionHandler];
}

#pragma mark -
#pragma mark Private Properties

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

- (NSArray*)allBrowsingDataManagers {
  DCHECK([NSThread isMainThread]);
  return [_browsingDataTypeMap allValues];
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

#pragma mark -
#pragma mark Private Class Methods

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
          setQualityOfService:NSQualityOfServiceUserInitiated];
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

#pragma mark -
#pragma mark Private Methods

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
}

- (SEL)browsingDataManagerSelectorForRemoveOperationType {
  if (self.mode == web::ACTIVE) {
    return @selector(removeDataAtCanonicalPath);
  }
  if (self.mode == web::INACTIVE) {
    return @selector(removeDataAtStashPath);
  }
  // Since the mode is |CHANGING|, find the last |STASH| or |RESTORE| operation
  // that was enqueued in order to find out the eventual mode that the
  // CRWBrowsingDataStore will be in when this |REMOVE| operation is run.
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

- (BOOL)finalizeChangeToMode:(web::BrowsingDataStoreMode)mode {
  DCHECK([NSThread isMainThread]);
  DCHECK_NE(web::CHANGING, mode);

  BOOL modeChangeWasSuccessful = NO;
  if (!self.numberOfPendingStashOrRestoreOperations) {
    [self setMode:mode];
    modeChangeWasSuccessful = YES;
  }
  return modeChangeWasSuccessful;
}

- (void)changeMode:(web::BrowsingDataStoreMode)mode
    completionHandler:
        (void (^)(BOOL modeChangeWasSuccessful))completionHandler {
  DCHECK([NSThread isMainThread]);

  ProceduralBlock completionHandlerAfterPerformingOperation = ^{
    BOOL modeChangeWasSuccessful = [self finalizeChangeToMode:mode];
    if (completionHandler) {
      completionHandler(modeChangeWasSuccessful);
    }
  };

  OperationType operationType = [self operationTypeToChangeMode:mode];
  if (operationType == NONE) {
    // As a caller of this API, it is awkward to get the callback before the
    // method call has completed, hence defer it.
    dispatch_async(dispatch_get_main_queue(),
                   completionHandlerAfterPerformingOperation);
  } else {
    [self performOperationWithType:operationType
              browsingDataManagers:[self allBrowsingDataManagers]
                 completionHandler:completionHandlerAfterPerformingOperation];
  }
}

- (OperationType)operationTypeToChangeMode:(web::BrowsingDataStoreMode)mode {
  DCHECK_NE(web::CHANGING, mode);

  OperationType operationType = NONE;
  if (mode == self.mode) {
    // Already in the desired mode.
    operationType = NONE;
  } else if (mode == web::ACTIVE) {
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
  return operationType;
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

  ProceduralBlock callCompletionHandlerOnMainThread = ^{
    // This is called on a background thread, hence the need to bounce to the
    // main thread.
    dispatch_async(dispatch_get_main_queue(), completionHandler);
  };
  NSOperation* operation =
      [self operationWithBrowsingDataManagers:browsingDataManagers
                                     selector:selector];

  if (operationType == RESTORE || operationType == STASH) {
    [operation setName:(RESTORE ? kRestoreOperationName : kStashOperationName)];
    _lastDispatchedStashOrRestoreOperation.reset([operation retain]);
  }

  NSOperationQueue* queue = nil;
  switch (operationType) {
    case NONE:
      NOTREACHED();
      break;
    case STASH:
    case RESTORE:
      queue = [CRWBrowsingDataStore operationQueueForStashAndRestoreOperations];
      break;
    case REMOVE:
      queue = [CRWBrowsingDataStore operationQueueForRemoveOperations];
      break;
  }

  NSOperation* completionHandlerOperation = [NSBlockOperation
      blockOperationWithBlock:callCompletionHandlerOnMainThread];

  [self addOperation:operation toQueue:queue];
  [self addOperation:completionHandlerOperation toQueue:queue];
}

- (NSOperation*)operationWithBrowsingDataManagers:(NSArray*)browsingDataManagers
                                         selector:(SEL)selector {
  NSBlockOperation* operation = [[[NSBlockOperation alloc] init] autorelease];
  for (id<CRWBrowsingDataManager> manager : browsingDataManagers) {
    // |addExecutionBlock| farms out the different blocks added to it. hence the
    // operations are implicitly parallelized.
    [operation addExecutionBlock:^{
      [manager performSelector:selector];
    }];
  }
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
