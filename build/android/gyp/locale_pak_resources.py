#!/usr/bin/env python
#
# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Creates a resources.zip for locale .pak files.

Places the locale.pak files into appropriate resource configs
(e.g. en-GB.pak -> res/raw-en/en_gb.pak). Also generates a locale_paks
TypedArray so that resource files can be enumerated at runtime.
"""

import collections
import optparse
import os
import sys
import zipfile

from util import build_utils


# This should stay in sync with:
# base/android/java/src/org/chromium/base/LocaleUtils.java
_CHROME_TO_ANDROID_LOCALE_MAP = {
    'he': 'iw',
    'id': 'in',
    'fil': 'tl',
}


def CreateLocalePaksXml(names):
  """Creates the contents for the locale-paks.xml files."""
  VALUES_FILE_TEMPLATE = '''<?xml version="1.0" encoding="utf-8"?>
<resources>
  <array name="locale_paks">%s
  </array>
</resources>
'''
  VALUES_ITEM_TEMPLATE = '''
    <item>@raw/%s</item>'''

  res_names = (os.path.splitext(name)[0] for name in names)
  items = ''.join((VALUES_ITEM_TEMPLATE % name for name in res_names))
  return VALUES_FILE_TEMPLATE % items


def main():
  parser = optparse.OptionParser()
  build_utils.AddDepfileOption(parser)
  parser.add_option('--locale-paks', help='List of files for res/raw-LOCALE')
  parser.add_option('--resources-zip', help='Path to output resources.zip')

  options, _ = parser.parse_args()
  build_utils.CheckOptions(options, parser,
                           required=['locale_paks', 'resources_zip'])

  sources = build_utils.ParseGypList(options.locale_paks)

  if options.depfile:
    deps = sources + build_utils.GetPythonDependencies()
    build_utils.WriteDepfile(options.depfile, deps)

  with zipfile.ZipFile(options.resources_zip, 'w', zipfile.ZIP_STORED) as out:
    # e.g. "en" -> ["en_gb.pak"]
    lang_to_locale_map = collections.defaultdict(list)
    for src_path in sources:
      basename = os.path.basename(src_path)
      name = os.path.splitext(basename)[0]
      # Resources file names must consist of [a-z0-9_.].
      res_compatible_name = basename.replace('-', '_').lower()
      if name == 'en-US':
        dest_dir = 'raw'
      else:
        # Chrome uses different region mapping logic from Android, so include
        # all regions for each language.
        android_locale = _CHROME_TO_ANDROID_LOCALE_MAP.get(name, name)
        lang = android_locale[0:2]
        dest_dir = 'raw-' + lang
        lang_to_locale_map[lang].append(res_compatible_name)
      out.write(src_path, os.path.join(dest_dir, res_compatible_name))

    # Create a String Arrays resource so ResourceExtractor can enumerate files.
    def WriteValuesFile(lang, names):
      dest_dir = 'values'
      if lang:
        dest_dir += '-' + lang
      # Always extract en-US.pak since it's the fallback.
      xml = CreateLocalePaksXml(names + ['en_us.pak'])
      out.writestr(os.path.join(dest_dir, 'locale-paks.xml'), xml)

    for lang, names in lang_to_locale_map.iteritems():
      WriteValuesFile(lang, names)
    WriteValuesFile(None, [])


if __name__ == '__main__':
  sys.exit(main())
