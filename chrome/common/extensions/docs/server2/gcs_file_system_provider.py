# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import environment

from caching_file_system import CachingFileSystem
from empty_dir_file_system import EmptyDirFileSystem
from extensions_paths import LOCAL_GCS_DIR, LOCAL_GCS_DEBUG_CONF
from local_file_system import LocalFileSystem
from path_util import IsDirectory, ToDirectory

class CloudStorageFileSystemProvider(object):
  '''Provides CloudStorageFileSystem bound to a GCS bucket.
  '''
  def __init__(self, object_store_creator):
    self._object_store_creator = object_store_creator

  def Create(self, bucket):
    '''Creates a CloudStorageFileSystemProvider.

    |bucket| is the name of GCS bucket, eg devtools-docs. It is expected
             that this bucket has Read permission for this app in its ACLs.

    Optional configuration can be set in a local_debug/gcs_debug.conf file:
      use_local_fs=True|False
      access_token=<token>
      remote_bucket_prefix=<prefix>

    If running in Preview mode or in Development mode with use_local_fs set to
    True, buckets and files are looked inside the local_debug folder instead
    of in the real GCS server. Preview server does not support direct GCS
    access, so it is always forced to use a LocalFileSystem.

    For real GCS access in the Development mode (dev_appserver.py),
    access_token and remote_bucket_prefix options can be
    used to change the way GCS files are accessed. Both are ignored in a real
    appengine instance.

    "access_token" is always REQUIRED on dev_appengine, otherwise you will
    get 404 (auth) errors. You can get one access_token valid for a few minutes
    by typing:
      gsutil -d ls 2>&1 | grep "Bearer" |
         sed "s/.*Bearer \(.*\).r.nUser-Agent.*/access_token=\1/" )"

    A sample output would be:
      access_token=ya29.1.AADtN_VW5ibbfLHV5cMIK5ss4bHtVzBXpa4byjd

    Now add this line to the local_debug/gcs_debug.conf file and restart the
    appengine development server.

    Remember that you will need a new access_token every ten minutes or
    so. If you get 404 errors on log, update it. Access token is not
    used for a deployed appengine app, only if you use dev_appengine.py.

    remote_bucket_prefix is useful if you want to test on your own GCS buckets
    before using the real GCS buckets.

    '''
    if not environment.IsReleaseServer() and not environment.IsDevServer():
      bucket_local_path = os.path.join(LOCAL_GCS_DIR, bucket)
      if IsDirectory(bucket_local_path):
        return LocalFileSystem(bucket_local_path)
      else:
        return EmptyDirFileSystem()

    debug_access_token = None
    debug_bucket_prefix = None
    use_local_fs = False

    if environment.IsDevServer() and os.path.exists(LOCAL_GCS_DEBUG_CONF):
      with open(LOCAL_GCS_DEBUG_CONF, "r") as token_file:
        properties = dict(line.strip().split('=', 1) for line in token_file)
      use_local_fs = properties.get('use_local_fs', 'False')=='True'
      debug_access_token = properties.get('access_token', None)
      debug_bucket_prefix = properties.get('remote_bucket_prefix', None)

    if environment.IsDevServer() and use_local_fs:
      return LocalFileSystem(ToDirectory(os.path.join(LOCAL_GCS_DIR, bucket)))

    # gcs_file_system has strong dependencies on runtime appengine APIs,
    # so we only import it when we are sure we are not on preview.py or tests.
    from gcs_file_system import CloudStorageFileSystem
    return CachingFileSystem(CloudStorageFileSystem(bucket,
        debug_access_token, debug_bucket_prefix),
        self._object_store_creator)

  @staticmethod
  def ForEmpty():
    class EmptyImpl(object):
      def Create(self, bucket):
        return EmptyDirFileSystem()
    return EmptyImpl()
