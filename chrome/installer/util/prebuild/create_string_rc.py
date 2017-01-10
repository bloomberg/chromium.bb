#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Generates .h and .rc files for installer strings. Run "python
create_string_rc.py" for usage details.

This script generates an rc file and header (NAME.{rc,h}) to be included in
setup.exe. The rc file includes translations for strings pulled from the given
.grd file(s) and their corresponding localized .xtb files.

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

import argparse
import glob
import io
import os
import sys
from xml import sax

BASEDIR = os.path.dirname(os.path.abspath(__file__))
sys.path.append(os.path.join(BASEDIR, '../../../../tools/grit'))
sys.path.append(os.path.join(BASEDIR, '../../../../tools/python'))

from grit.extern import tclib

# The IDs of strings we want to import from the .grd files and include in
# setup.exe's resources.
STRING_IDS = [
  'IDS_PRODUCT_NAME',
  'IDS_SXS_SHORTCUT_NAME',
  'IDS_PRODUCT_DESCRIPTION',
  'IDS_ABOUT_VERSION_COMPANY_NAME',
  'IDS_INSTALL_HIGHER_VERSION',
  'IDS_INSTALL_FAILED',
  'IDS_SAME_VERSION_REPAIR_FAILED',
  'IDS_SETUP_PATCH_FAILED',
  'IDS_INSTALL_OS_NOT_SUPPORTED',
  'IDS_INSTALL_OS_ERROR',
  'IDS_INSTALL_SINGLETON_ACQUISITION_FAILED',
  'IDS_INSTALL_TEMP_DIR_FAILED',
  'IDS_INSTALL_UNCOMPRESSION_FAILED',
  'IDS_INSTALL_INVALID_ARCHIVE',
  'IDS_INSTALL_INSUFFICIENT_RIGHTS',
  'IDS_SHORTCUT_TOOLTIP',
  'IDS_SHORTCUT_NEW_WINDOW',
  'IDS_APP_SHORTCUTS_SUBDIR_NAME',
  'IDS_APP_SHORTCUTS_SUBDIR_NAME_CANARY',
  'IDS_INBOUND_MDNS_RULE_NAME',
  'IDS_INBOUND_MDNS_RULE_NAME_CANARY',
  'IDS_INBOUND_MDNS_RULE_DESCRIPTION',
  'IDS_INBOUND_MDNS_RULE_DESCRIPTION_CANARY',
  'IDS_INSTALL_EXISTING_VERSION_LAUNCHED',
]

# The ID of the first resource string.
FIRST_RESOURCE_ID = 1600


class GrdHandler(sax.handler.ContentHandler):
  """Extracts selected strings from a .grd file.

  Attributes:
    messages: A dict mapping string identifiers to their corresponding messages.
  """
  def __init__(self, string_ids):
    """Constructs a handler that reads selected strings from a .grd file.

    The dict attribute |messages| is populated with the strings that are read.

    Args:
      string_ids: A list of message identifiers to extract.
    """
    sax.handler.ContentHandler.__init__(self)
    self.messages = {}
    self.__id_set = set(string_ids)
    self.__message_name = None
    self.__element_stack = []
    self.__text_scraps = []
    self.__characters_callback = None

  def startElement(self, name, attrs):
    self.__element_stack.append(name)
    if name == 'message':
      self.__OnOpenMessage(attrs.getValue('name'))

  def endElement(self, name):
    popped = self.__element_stack.pop()
    assert popped == name
    if name == 'message':
      self.__OnCloseMessage()

  def characters(self, content):
    if self.__characters_callback:
      self.__characters_callback(self.__element_stack[-1], content)

  def __IsExtractingMessage(self):
    """Returns True if a message is currently being extracted."""
    return self.__message_name is not None

  def __OnOpenMessage(self, message_name):
    """Invoked at the start of a <message> with message's name."""
    assert not self.__IsExtractingMessage()
    self.__message_name = (message_name if message_name in self.__id_set
                           else None)
    if self.__message_name:
      self.__characters_callback = self.__OnMessageText

  def __OnMessageText(self, containing_element, message_text):
    """Invoked to handle a block of text for a message."""
    if message_text and (containing_element == 'message' or
                         containing_element == 'ph'):
      self.__text_scraps.append(message_text)

  def __OnCloseMessage(self):
    """Invoked at the end of a message."""
    if self.__IsExtractingMessage():
      self.messages[self.__message_name] = ''.join(self.__text_scraps).strip()
      self.__message_name = None
      self.__text_scraps = []
      self.__characters_callback = None


class XtbHandler(sax.handler.ContentHandler):
  """Extracts selected translations from an .xrd file.

  Populates the |lang| and |translations| attributes with the language and
  selected strings of an .xtb file. Instances may be re-used to read the same
  set of translations from multiple .xtb files.

  Attributes:
    translations: A mapping of translation ids to strings.
    lang: The language parsed from the .xtb file.
  """
  def __init__(self, translation_ids):
    """Constructs an instance to parse the given strings from an .xtb file.

    Args:
      translation_ids: a mapping of translation ids to their string
        identifiers list for the translations to be extracted.
    """
    sax.handler.ContentHandler.__init__(self)
    self.lang = None
    self.translations = None
    self.__translation_ids = translation_ids
    self.__element_stack = []
    self.__string_ids = None
    self.__text_scraps = []
    self.__characters_callback = None

  def startDocument(self):
    # Clear the lang and translations since a new document is being parsed.
    self.lang = ''
    self.translations = {}

  def startElement(self, name, attrs):
    self.__element_stack.append(name)
    # translationbundle is the document element, and hosts the lang id.
    if len(self.__element_stack) == 1:
      assert name == 'translationbundle'
      self.__OnLanguage(attrs.getValue('lang'))
    if name == 'translation':
      self.__OnOpenTranslation(attrs.getValue('id'))

  def endElement(self, name):
    popped = self.__element_stack.pop()
    assert popped == name
    if name == 'translation':
      self.__OnCloseTranslation()

  def characters(self, content):
    if self.__characters_callback:
      self.__characters_callback(self.__element_stack[-1], content)

  def __OnLanguage(self, lang):
    self.lang = lang.replace('-', '_').upper()

  def __OnOpenTranslation(self, translation_id):
    assert self.__string_ids is None
    self.__string_ids = self.__translation_ids.get(translation_id)
    if self.__string_ids:
      self.__characters_callback = self.__OnTranslationText

  def __OnTranslationText(self, containing_element, message_text):
    if message_text and containing_element == 'translation':
      self.__text_scraps.append(message_text)

  def __OnCloseTranslation(self):
    if self.__string_ids:
      translated_string = ''.join(self.__text_scraps).strip()
      for string_id in self.__string_ids:
        self.translations[string_id] = translated_string
      self.__string_ids = None
      self.__text_scraps = []
      self.__characters_callback = None


class StringRcMaker(object):
  """Makes .h and .rc files containing strings and translations."""
  def __init__(self, name, inputs, outdir):
    """Constructs a maker.

    Args:
      name: The base name of the generated files (e.g.,
        'installer_util_strings').
      inputs: A list of (grd_file, xtb_dir) pairs containing the source data.
      outdir: The directory into which the files will be generated.
    """
    self.name = name
    self.inputs = inputs
    self.outdir = outdir

  def MakeFiles(self):
    translated_strings = self.__ReadSourceAndTranslatedStrings()
    self.__WriteRCFile(translated_strings)
    self.__WriteHeaderFile(translated_strings)

  class __TranslationData(object):
    """A container of information about a single translation."""
    def __init__(self, resource_id_str, language, translation):
      self.resource_id_str = resource_id_str
      self.language = language
      self.translation = translation

    def __cmp__(self, other):
      """Allow __TranslationDatas to be sorted by id then by language."""
      id_result = cmp(self.resource_id_str, other.resource_id_str)
      return cmp(self.language, other.language) if id_result == 0 else id_result

  def __ReadSourceAndTranslatedStrings(self):
    """Reads the source strings and translations from all inputs."""
    translated_strings = []
    for grd_file, xtb_dir in self.inputs:
      # Get the name of the grd file sans extension.
      source_name = os.path.splitext(os.path.basename(grd_file))[0]
      # Compute a glob for the translation files.
      xtb_pattern = os.path.join(os.path.dirname(grd_file), xtb_dir,
                                 '%s*.xtb' % source_name)
      translated_strings.extend(
        self.__ReadSourceAndTranslationsFrom(grd_file, glob.glob(xtb_pattern)))
    translated_strings.sort()
    return translated_strings

  def __ReadSourceAndTranslationsFrom(self, grd_file, xtb_files):
    """Reads source strings and translations for a .grd file.

    Reads the source strings and all available translations for the messages
    identified by STRING_IDS. The source string is used where translations are
    missing.

    Args:
      grd_file: Path to a .grd file.
      xtb_files: List of paths to .xtb files.

    Returns:
      An unsorted list of __TranslationData instances.
    """
    sax_parser = sax.make_parser()

    # Read the source (en-US) string from the .grd file.
    grd_handler = GrdHandler(STRING_IDS)
    sax_parser.setContentHandler(grd_handler)
    sax_parser.parse(grd_file)
    source_strings = grd_handler.messages

    # Manually put the source strings as en-US in the list of translated
    # strings.
    translated_strings = []
    for string_id, message_text in source_strings.iteritems():
      translated_strings.append(self.__TranslationData(string_id,
                                                       'EN_US',
                                                       message_text))

    # Generate the message ID for each source string to correlate it with its
    # translations in the .xtb files. Multiple source strings may have the same
    # message text; hence the message id is mapped to a list of string ids
    # instead of a single value.
    translation_ids = {}
    for (string_id, message_text) in source_strings.iteritems():
      message_id = tclib.GenerateMessageId(message_text)
      translation_ids.setdefault(message_id, []).append(string_id);

    # Gather the translated strings from the .xtb files. Use the en-US string
    # for any message lacking a translation.
    xtb_handler = XtbHandler(translation_ids)
    sax_parser.setContentHandler(xtb_handler)
    for xtb_filename in xtb_files:
      sax_parser.parse(xtb_filename)
      for string_id, message_text in source_strings.iteritems():
        translated_string = xtb_handler.translations.get(string_id,
                                                         message_text)
        translated_strings.append(self.__TranslationData(string_id,
                                                         xtb_handler.lang,
                                                         translated_string))
    return translated_strings

  def __WriteRCFile(self, translated_strings):
    """Writes a resource file with the strings provided in |translated_strings|.
    """
    HEADER_TEXT = (
      u'#include "%s.h"\n\n'
      u'STRINGTABLE\n'
      u'BEGIN\n'
      ) % self.name

    FOOTER_TEXT = (
      u'END\n'
    )

    with io.open(os.path.join(self.outdir, self.name + '.rc'),
                 mode='w',
                 encoding='utf-16',
                 newline='\n') as outfile:
      outfile.write(HEADER_TEXT)
      for translation in translated_strings:
        # Escape special characters for the rc file.
        escaped_text = (translation.translation.replace('"', '""')
                       .replace('\t', '\\t')
                       .replace('\n', '\\n'))
        outfile.write(u'  %s "%s"\n' %
                      (translation.resource_id_str + '_' + translation.language,
                       escaped_text))
      outfile.write(FOOTER_TEXT)

  def __WriteHeaderFile(self, translated_strings):
    """Writes a .h file with resource ids."""
    # TODO(grt): Stream the lines to the file rather than building this giant
    # list of lines first.
    lines = []
    do_languages_lines = ['\n#define DO_LANGUAGES']
    installer_string_mapping_lines = ['\n#define DO_INSTALLER_STRING_MAPPING']

    # Write the values for how the languages ids are offset.
    seen_languages = set()
    offset_id = 0
    for translation_data in translated_strings:
      lang = translation_data.language
      if lang not in seen_languages:
        seen_languages.add(lang)
        lines.append('#define IDS_L10N_OFFSET_%s %s' % (lang, offset_id))
        do_languages_lines.append('  HANDLE_LANGUAGE(%s, IDS_L10N_OFFSET_%s)'
                                  % (lang.replace('_', '-').lower(), lang))
        offset_id += 1
      else:
        break

    # Write the resource ids themselves.
    resource_id = FIRST_RESOURCE_ID
    for translation_data in translated_strings:
      lines.append('#define %s %s' % (translation_data.resource_id_str + '_' +
                                      translation_data.language,
                                      resource_id))
      resource_id += 1

    # Write out base ID values.
    for string_id in STRING_IDS:
      lines.append('#define %s_BASE %s_%s' % (string_id,
                                              string_id,
                                              translated_strings[0].language))
      installer_string_mapping_lines.append('  HANDLE_STRING(%s_BASE, %s)'
                                            % (string_id, string_id))

    with open(os.path.join(self.outdir, self.name + '.h'), 'wb') as outfile:
      outfile.write('\n'.join(lines))
      outfile.write('\n#ifndef RC_INVOKED')
      outfile.write(' \\\n'.join(do_languages_lines))
      outfile.write(' \\\n'.join(installer_string_mapping_lines))
      # .rc files must end in a new line
      outfile.write('\n#endif  // ndef RC_INVOKED\n')


def ParseCommandLine():
  def GrdPathAndXtbDirPair(string):
    """Returns (grd_path, xtb_dir) given a colon-separated string of the same.
    """
    parts = string.split(':')
    if len(parts) is not 2:
      raise argparse.ArgumentTypeError('%r is not grd_path:xtb_dir')
    return (parts[0], parts[1])

  parser = argparse.ArgumentParser(
    description='Generate .h and .rc files for installer strings.')
  parser.add_argument('-i', action='append',
                      type=GrdPathAndXtbDirPair,
                      required=True,
                      help='path to .grd file:relative path to .xtb dir',
                      metavar='GRDFILE:XTBDIR',
                      dest='inputs')
  parser.add_argument('-o',
                      required=True,
                      help='output directory for generated .rc and .h files',
                      dest='outdir')
  parser.add_argument('-n',
                      required=True,
                      help='base name of generated .rc and .h files',
                      dest='name')
  return parser.parse_args()


def main():
  args = ParseCommandLine()
  StringRcMaker(args.name, args.inputs, args.outdir).MakeFiles()
  return 0


if '__main__' == __name__:
  sys.exit(main())
