# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import manifest_util

def Info(manifest, bundle_names):
  valid_bundles = [bundle.name for bundle in manifest.GetBundles()]
  valid_bundles = set(bundle_names) & set(valid_bundles)
  invalid_bundles = set(bundle_names) - valid_bundles
  if invalid_bundles:
    logging.warn('Unknown bundle(s): %s\n' % (', '.join(invalid_bundles)))

  for bundle_name in bundle_names:
    if bundle_name not in valid_bundles:
      continue

    bundle = manifest.GetBundle(bundle_name)

    print bundle.name
    for key in sorted(bundle.iterkeys()):
      value = bundle[key]
      if key == manifest_util.ARCHIVES_KEY:
        archive = bundle.GetHostOSArchive()
        print '  Archive:'
        if archive:
          for archive_key in sorted(archive.iterkeys()):
            print '    %s: %s' % (archive_key, archive[archive_key])
        else:
          print '    No archives for this host.'
      elif key not in (manifest_util.ARCHIVES_KEY, manifest_util.NAME_KEY):
        print '  %s: %s' % (key, value)
    print
