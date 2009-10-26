// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_WORKER_POOL_MAC_H_
#define BASE_WORKER_POOL_MAC_H_

#include "base/worker_pool.h"

#import <Foundation/Foundation.h>

// WorkerPoolObjC provides an Objective-C interface to the same WorkerPool
// used by the rest of the application.
@interface WorkerPoolObjC : NSObject

// Returns the underlying NSOperationQueue back end that WorkerPool::PostTask
// would post tasks to.  This can be used to add NSOperation subclasses
// directly to the same NSOperationQueue, by calling -[NSOperationQueue
// addOperation:].  Most Objective-C code wishing to dispatch tasks to the
// WorkerPool will find it handy to add an operation of type
// NSInvocationOperation.
+ (NSOperationQueue*)sharedOperationQueue;

@end  // @interface WorkerPoolObjC

#endif  // BASE_WORKER_POOL_MAC_H_
