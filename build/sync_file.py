import os.path

import http_download


def SyncFile(url, target, username=None, password=None, verbose=True):
  filename = os.path.join(target, os.path.basename(url))
  if verbose:
    print 'Downloading %s to %s...' % (url, filename)
  http_download.HttpDownload(url, filename,
                             username=username, password=password)
