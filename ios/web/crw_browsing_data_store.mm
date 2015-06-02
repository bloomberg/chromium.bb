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
  STASH = 0,
  RESTORE,
  REMOVE
};
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

// The array of all browsing data managers. Must be accessed from the main
// thread.
@property(nonatomic, readonly) NSArray* allBrowsingDataManagers;
// Returns an array of browsing data managers for the given |browsingDataTypes|.
- (NSArray*)browsingDataManagersForBrowsingDataTypes:
        (web::BrowsingDataTypes)browsingDataTypes;

// Redefined to be read-write. Must be called from the main thread.
@property(nonatomic, assign) CRWBrowsingDataStoreMode mode;
// Sets the mode iff there are no more stash or restore operations that are
// still pending. |mode| can only be either |ACTIVE| or |INACTIVE|.
// |handler| is called immediately (in the same runloop) with a BOOL indicating
// whether the mode change was successful or not. |handler| can be nil.
- (void)finalizeChangeToMode:(CRWBrowsingDataStoreMode)mode
    andCallCompletionHandler:(void (^)(BOOL modeChangeWasSuccessful))handler;

// The number of stash or restore operations that are still pending.
@property(nonatomic, assign) NSUInteger numberOfPendingStashOrRestoreOperations;

// Performs operations of type |operationType| on each of the
// |browsingDataManagers|.
// The kind of operations are:
// 1) STASH: Stash operation involves stashing browsing data that web views
// (UIWebViews and WKWebViews) create to the associated BrowserState's state
// path.
// 2) RESTORE: Restore operation involves restoring browsing data from the
// associated BrowserState's state path so that web views (UIWebViews and
// WKWebViews) can read from them.
// 3) REMOVE: Remove operation involves removing the data that web views
// (UIWebViews and WKWebViews) create.
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
  CRWBrowsingDataStoreMode _mode;
  // The dictionary that maps a browsing data type to its
  // CRWBrowsingDataManager.
  base::scoped_nsobject<NSDictionary> _browsingDataTypeMap;
  // The last operation that was enqueued to be run. Can be stash, restore or a
  // delete operation.
  base::scoped_nsobject<NSOperation> _lastDispatchedOperation;
  // The number of stash or restore operations that are still pending. If this
  // value > 0 the mode of the CRWBrowsingDataStore is SYNCHRONIZING. The mode
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
    DCHECK(web::BrowserState::GetActiveStateManager(browserState)->IsActive());
    DCHECK([NSThread isMainThread]);
    // TODO(shreyasv): Instantiate the necessary CRWBrowsingDataManagers that
    // are encapsulated within this class. crbug.com/480654.
    _browserState = browserState;
    _mode = ACTIVE;
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

- (CRWBrowsingDataStoreMode)mode {
  DCHECK([NSThread isMainThread]);
  return _mode;
}

- (void)setMode:(CRWBrowsingDataStoreMode)mode {
  DCHECK([NSThread isMainThread]);
  if (_mode == mode) {
    return;
  }
  if (mode == ACTIVE || mode == INACTIVE) {
    DCHECK(!self.numberOfPendingStashOrRestoreOperations);
  }
  [self willChangeValueForKey:@"mode"];
  _mode = mode;
  [self didChangeValueForKey:@"mode"];
}

- (void)finalizeChangeToMode:(CRWBrowsingDataStoreMode)mode
    andCallCompletionHandler:(void (^)(BOOL modeChangeWasSuccessful))handler {
  DCHECK([NSThread isMainThread]);
  DCHECK_NE(SYNCHRONIZING, mode);

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

  // TODO(shreyasv): Consult the delegate here if it wants to |DELETE| instead.
  // crbug.com/480654.

  base::WeakNSObject<CRWBrowsingDataStore> weakSelf(self);
  [self performOperationWithType:RESTORE
            browsingDataManagers:[self allBrowsingDataManagers]
               completionHandler:^{
                 [weakSelf finalizeChangeToMode:ACTIVE
                       andCallCompletionHandler:completionHandler];
               }];
}

- (void)makeInactiveWithCompletionHandler:
        (void (^)(BOOL success))completionHandler {
  DCHECK([NSThread isMainThread]);

  // TODO(shreyasv): Consult the delegate here if it wants to |DELETE| instead.
  // crbug.com/480654.

  base::WeakNSObject<CRWBrowsingDataStore> weakSelf(self);
  [self performOperationWithType:STASH
            browsingDataManagers:[self allBrowsingDataManagers]
               completionHandler:^{
                 [weakSelf finalizeChangeToMode:INACTIVE
                       andCallCompletionHandler:completionHandler];
               }];
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

  SEL selector = nullptr;
  switch (operationType) {
    case STASH:
      selector = @selector(stashData);
      break;
    case RESTORE:
      selector = @selector(restoreData);
      break;
    case REMOVE:
      selector = @selector(removeData);
      break;
    default:
      NOTREACHED();
      break;
  };

  if (operationType == RESTORE || operationType == STASH) {
    [self setMode:SYNCHRONIZING];
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
