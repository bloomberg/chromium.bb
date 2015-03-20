// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/webp_transcode/webp_network_client_factory.h"

#include "base/logging.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#import "components/webp_transcode/webp_network_client.h"

@interface WebPNetworkClientFactory () {
  scoped_refptr<base::SequencedTaskRunner> _taskRunner;
}
@end

@implementation WebPNetworkClientFactory

- (instancetype)init {
  NOTREACHED() << "Use |-initWithTaskRunner:| instead";
  return nil;
}

- (Class)clientClass {
  return [WebPNetworkClient class];
}

- (instancetype)initWithTaskRunner:
    (const scoped_refptr<base::SequencedTaskRunner>&)runner {
  if ((self = [super init])) {
    DCHECK(runner);
    _taskRunner = runner;
  }
  return self;
}

- (CRNForwardingNetworkClient*)clientHandlingAnyRequest {
  return
      [[[WebPNetworkClient alloc] initWithTaskRunner:_taskRunner] autorelease];
}

@end
