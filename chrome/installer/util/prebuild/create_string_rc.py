#!/usr/bin/python
# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This script generates an rc file and header (setup_strings.{rc,h}) to be
included in setup.exe. The rc file includes translations for strings pulled
from generated_resource.grd and the localized .xtb files.

The header file includes IDs for each string, but also has values to allow
getting a string based on a language offset.  For example, the header file
looks like this:

#define IDS_L10N_OFFSET_AR 0
#define IDS_L10N_OFFSET_BG 1
#define IDS_L10N_OFFSET_CA 2
...
#define IDS_L10N_OFFSET_ZH_TW 41

#define IDS_MY_STRING_AR 1600
#define IDS_MY_STRING_BG 1601
...
#define IDS_MY_STRING_BASE IDS_MY_STRING_AR

This allows us to lookup an an ID for a string by adding IDS_MY_STRING_BASE and
IDS_L10N_OFFSET_* for the language we are interested in.
"""

import glob
import os
import sys
from xml.dom import minidom

# We are expected to use ../../../../third_party/python_24/python.exe
from google import path_utils

# Quick hack to fix the path.
sys.path.append(os.path.abspath('../../tools/grit/grit/extern'))
sys.path.append(os.path.abspath('../tools/grit/grit/extern'))
import FP

# The IDs of strings we want to import from generated_resources.grd and include
# in setup.exe's resources.
kStringIds = [
  'IDS_PRODUCT_NAME',
  'IDS_SXS_SHORTCUT_NAME',
  'IDS_PRODUCT_DESCRIPTION',
  'IDS_PRODUCT_FRAME_NAME',
  'IDS_UNINSTALL_CHROME',
  'IDS_ABOUT_VERSION_COMPANY_NAME',
  'IDS_INSTALL_HIGHER_VERSION',
  'IDS_INSTALL_HIGHER_VERSION_CF',
  'IDS_INSTALL_USER_LEVEL_EXISTS',
  'IDS_INSTALL_SYSTEM_LEVEL_EXISTS',
  'IDS_INSTALL_FAILED',
  'IDS_SAME_VERSION_REPAIR_FAILED',
  'IDS_SAME_VERSION_REPAIR_FAILED_CF',
  'IDS_SETUP_PATCH_FAILED',
  'IDS_INSTALL_OS_NOT_SUPPORTED',
  'IDS_INSTALL_OS_ERROR',
  'IDS_INSTALL_TEMP_DIR_FAILED',
  'IDS_INSTALL_UNCOMPRESSION_FAILED',
  'IDS_INSTALL_INVALID_ARCHIVE',
  'IDS_INSTALL_INSUFFICIENT_RIGHTS',
  'IDS_UNINSTALL_FAILED',
  'IDS_UNINSTALL_COMPLETE',
  'IDS_INSTALL_DIR_IN_USE',
  'IDS_INSTALL_NON_MULTI_INSTALLATION_EXISTS',
  'IDS_INSTALL_MULTI_INSTALLATION_EXISTS',
  'IDS_INSTALL_CONFLICTING_CHANNEL_EXISTS',
  'IDS_INSTALL_READY_MODE_REQUIRES_CHROME',
  'IDS_OEM_MAIN_SHORTCUT_NAME',
  'IDS_SHORTCUT_TOOLTIP',
]

# The ID of the first resource string.
kFirstResourceID = 1600

class TranslationStruct:
  """A helper struct that holds information about a single translation."""
  def __init__(self, resource_id_str, language, translation):
    self.resource_id_str = resource_id_str
    self.language = language
    self.translation = translation

  def __cmp__(self, other):
    """Allow TranslationStructs to be sorted by id."""
    id_result = cmp(self.resource_id_str, other.resource_id_str)
    return cmp(self.language, other.language) if id_result == 0 else id_result


def CollectTranslatedStrings(branding):
  """Collects all the translations for all the strings specified by kStringIds.
  Returns a list of tuples of (string_id, language, translated string). The
  list is sorted by language codes."""
  strings_file = 'app/chromium_strings.grd'
  translation_files = 'chromium_strings*.xtb'
  if branding == 'Chrome':
    strings_file = 'app/google_chrome_strings.grd'
    translation_files = 'google_chrome_strings*.xtb'
  kGeneratedResourcesPath = os.path.join(path_utils.ScriptDir(), '..', '..',
                                         '..', strings_file)
  kTranslationDirectory = os.path.join(path_utils.ScriptDir(), '..', '..',
                                       '..', 'app', 'resources')
  kTranslationFiles = glob.glob(os.path.join(kTranslationDirectory,
                                             translation_files))

  # Get the strings out of generated_resources.grd.
  dom = minidom.parse(kGeneratedResourcesPath)
  # message_nodes is a list of message dom nodes corresponding to the string
  # ids we care about.  We want to make sure that this list is in the same
  # order as kStringIds so we can associate them together.
  message_nodes = []
  all_message_nodes = dom.getElementsByTagName('message')
  for string_id in kStringIds:
    message_nodes.append([x for x in all_message_nodes if
                          x.getAttribute('name') == string_id][0])
  message_texts = [node.firstChild.nodeValue.strip() for node in message_nodes]

  # The fingerprint of the string is the message ID in the translation files
  # (xtb files).
  translation_ids = [str(FP.FingerPrint(text)) for text in message_texts]

  # Manually put _EN_US in the list of translated strings because it doesn't
  # have a .xtb file.
  translated_strings = []
  for string_id, message_text in zip(kStringIds, message_texts):
    translated_strings.append(TranslationStruct(string_id,
                                                'EN_US',
                                                message_text))

  # Gather the translated strings from the .xtb files.  If an .xtb file doesn't
  # have the string we want, use the en-US string.
  for xtb_filename in kTranslationFiles:
    dom = minidom.parse(xtb_filename)
    language = dom.documentElement.getAttribute('lang')
    language = language.replace('-', '_').upper()
    translation_nodes = {}
    for translation_node in dom.getElementsByTagName('translation'):
      translation_id = translation_node.getAttribute('id')
      if translation_id in translation_ids:
        translation_nodes[translation_id] = (translation_node.firstChild
                                                             .nodeValue
                                                             .strip())
    for i, string_id in enumerate(kStringIds):
      translated_string = translation_nodes.get(translation_ids[i],
                                                message_texts[i])
      translated_strings.append(TranslationStruct(string_id,
                                                  language,
                                                  translated_string))

  translated_strings.sort()
  return translated_strings

def WriteRCFile(translated_strings, out_filename):
  """Writes a resource (rc) file with all the language strings provided in
  |translated_strings|."""
  kHeaderText = (
    u'#include "%s.h"\n\n'
    u'STRINGTABLE\n'
    u'BEGIN\n'
  ) % os.path.basename(out_filename)
  kFooterText = (
    u'END\n'
  )
  lines = [kHeaderText]
  for translation_struct in translated_strings:
    # Escape special characters for the rc file.
    translation = (translation_struct.translation.replace('"', '""')
                                                 .replace('\t', '\\t')
                                                 .replace('\n', '\\n'))
    lines.append(u'  %s "%s"\n' % (translation_struct.resource_id_str + '_'
                                       + translation_struct.language,
                                   translation))
  lines.append(kFooterText)
  outfile = open(out_filename + '.rc', 'wb')
  outfile.write(''.join(lines).encode('utf-16'))
  outfile.close()

def WriteHeaderFile(translated_strings, out_filename):
  """Writes a .h file with resource ids.  This file can be included by the
  executable to refer to identifiers."""
  lines = []
  do_languages_lines = ['#define DO_LANGUAGES']

  # Write the values for how the languages ids are offset.
  seen_languages = set()
  offset_id = 0
  for translation_struct in translated_strings:
    lang = translation_struct.language
    if lang not in seen_languages:
      seen_languages.add(lang)
      lines.append('#define IDS_L10N_OFFSET_%s %s' % (lang, offset_id))
      do_languages_lines.append('  HANDLE_LANGUAGE(%s, IDS_L10N_OFFSET_%s)'
                                % (lang.replace('_', '-').lower(), lang))
      offset_id += 1
    else:
      break

  # Write the resource ids themselves.
  resource_id = kFirstResourceID
  for translation_struct in translated_strings:
    lines.append('#define %s %s' % (translation_struct.resource_id_str + '_'
                                        + translation_struct.language,
                                    resource_id))
    resource_id += 1

  # Write out base ID values.
  for string_id in kStringIds:
    lines.append('#define %s_BASE %s_%s' % (string_id,
                                            string_id,
                                            translated_strings[0].language))

  outfile = open(out_filename + '.h', 'wb')
  outfile.write('\n'.join(lines))
  outfile.write('\n#ifndef RC_INVOKED\n')
  outfile.write(' \\\n'.join(do_languages_lines))
  # .rc files must end in a new line
  outfile.write('\n#endif  // ndef RC_INVOKED\n')
  outfile.close()

def main(argv):
  branding = ''
  if (len(sys.argv) > 2):
    branding = argv[2]
  translated_strings = CollectTranslatedStrings(branding)
  kFilebase = os.path.join(argv[1], 'installer_util_strings')
  WriteRCFile(translated_strings, kFilebase)
  WriteHeaderFile(translated_strings, kFilebase)

if '__main__' == __name__:
  if len(sys.argv) < 2:
    print 'Usage:\n  %s <output_directory> [branding]' % sys.argv[0]
    sys.exit(1)
  # Use optparse to parse command line flags.
  main(sys.argv)
