# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from pylib.remote.device import remote_device_environment

def CreateEnvironment(args, error_func):

  # TODO(jbudorick) Add local device environment.
  # TODO(jbudorick) Add local machine environment.

  if args.environment == 'remote_device':
    return remote_device_environment.RemoteDeviceEnvironment(args,
                                                             error_func)
  error_func('Unable to create %s environment.' % args.environment)
