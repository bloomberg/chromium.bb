#!/usr/bin/python
# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Extract UserMetrics "actions" strings from the Chrome source.

This program generates the list of known actions we expect to see in the
user behavior logs.  It walks the Chrome source, looking for calls to
UserMetrics functions, extracting actions and warning on improper calls,
as well as generating the lists of possible actions in situations where
there are many possible actions.

See also:
  chrome/browser/user_metrics.h
  http://wiki.corp.google.com/twiki/bin/view/Main/ChromeUserExperienceMetrics

Run it from the chrome/tools directory like:
  export PYTHONPATH=../../tools/python  # for google.path_utils
  extract_actions.py
or
  extract_actions.py --hash
  which will update the chromeactions.txt
"""

__author__ = 'evanm (Evan Martin)'

import os
import re
import sys
import hashlib

from google import path_utils

# Files that are known to use UserMetrics::RecordComputedAction(), which means
# they require special handling code in this script.
# To add a new file, add it to this list and add the appropriate logic to
# generate the known actions to AddComputedActions() below.
KNOWN_COMPUTED_USERS = (
  'back_forward_menu_model.cc',
  'options_page_view.cc',
  'render_view_host.cc',  # called using webkit identifiers
  'user_metrics.cc',  # method definition
  'new_tab_ui.cc',  # most visited clicks 1-9
  'extension_metrics_module.cc', # extensions hook for user metrics
  'safe_browsing_blocking_page.cc', # various interstitial types and actions
  'language_options_handler.cc', # languages and input methods in chrome os
)

# Language codes used in Chrome. The list should be updated when a new
# language is added to app/l10n_util.cc, as follows:
#
# % (cat app/l10n_util.cc | \
#    perl -n0e 'print $1 if /kAcceptLanguageList.*?\{(.*?)\}/s' | \
#    perl -nle 'print $1, if /"(.*)"/'; echo 'es-419') | \
#   sort | perl -pe "s/(.*)\n/'\$1', /" | \
#   fold -w75 -s | perl -pe 's/^/  /;s/ $//'; echo
#
# The script extracts language codes from kAcceptLanguageList, but es-419
# (Spanish in Latin America) is an exception.
LANGUAGE_CODES = (
  'af', 'am', 'ar', 'az', 'be', 'bg', 'bh', 'bn', 'br', 'bs', 'ca', 'co',
  'cs', 'cy', 'da', 'de', 'de-AT', 'de-CH', 'de-DE', 'el', 'en', 'en-AU',
  'en-CA', 'en-GB', 'en-NZ', 'en-US', 'en-ZA', 'eo', 'es', 'es-419', 'et',
  'eu', 'fa', 'fi', 'fil', 'fo', 'fr', 'fr-CA', 'fr-CH', 'fr-FR', 'fy',
  'ga', 'gd', 'gl', 'gn', 'gu', 'ha', 'haw', 'he', 'hi', 'hr', 'hu', 'hy',
  'ia', 'id', 'is', 'it', 'it-CH', 'it-IT', 'ja', 'jw', 'ka', 'kk', 'km',
  'kn', 'ko', 'ku', 'ky', 'la', 'ln', 'lo', 'lt', 'lv', 'mk', 'ml', 'mn',
  'mo', 'mr', 'ms', 'mt', 'nb', 'ne', 'nl', 'nn', 'no', 'oc', 'om', 'or',
  'pa', 'pl', 'ps', 'pt', 'pt-BR', 'pt-PT', 'qu', 'rm', 'ro', 'ru', 'sd',
  'sh', 'si', 'sk', 'sl', 'sn', 'so', 'sq', 'sr', 'st', 'su', 'sv', 'sw',
  'ta', 'te', 'tg', 'th', 'ti', 'tk', 'to', 'tr', 'tt', 'tw', 'ug', 'uk',
  'ur', 'uz', 'vi', 'xh', 'yi', 'yo', 'zh', 'zh-CN', 'zh-TW', 'zu',
)

# Input method IDs used in Chrome OS. The list should be updated when a
# new input method is added to platform/assets/input_methods/whitelist.txt
# in the Chrome OS tree, as follows:
#
# % sort chromeos/src/platform/assets/input_methods/whitelist.txt | \
#   perl -ne "print \"'\$1', \" if /^([^#]+?)\s/" | \
#   fold -w75 -s | perl -pe 's/^/  /;s/ $//'; echo
#
# The script extracts input method IDs from whitelist.txt.
INPUT_METHOD_IDS = (
  'chewing', 'hangul', 'm17n:ar:kbd', 'm17n:fa:isiri', 'm17n:hi:itrans',
  'm17n:th:kesmanee', 'm17n:th:pattachote', 'm17n:th:tis820',
  'm17n:vi:tcvn', 'm17n:vi:telex', 'm17n:vi:viqr', 'm17n:vi:vni',
  'm17n:zh:cangjie', 'm17n:zh:quick', 'mozc', 'mozc-dv', 'mozc-jp',
  'pinyin', 'xkb:be::fra', 'xkb:be::ger', 'xkb:be::nld', 'xkb:bg::bul',
  'xkb:bg:phonetic:bul', 'xkb:br::por', 'xkb:ca::fra', 'xkb:ca:eng:eng',
  'xkb:ch::ger', 'xkb:ch:fr:fra', 'xkb:cz::cze', 'xkb:de::ger',
  'xkb:dk::dan', 'xkb:ee::est', 'xkb:es::spa', 'xkb:es:cat:cat',
  'xkb:fi::fin', 'xkb:fr::fra', 'xkb:gb:extd:eng', 'xkb:gr::gre',
  'xkb:hr::scr', 'xkb:hu::hun', 'xkb:il::heb', 'xkb:it::ita', 'xkb:jp::jpn',
  'xkb:kr:kr104:kor', 'xkb:lt::lit', 'xkb:lv::lav', 'xkb:nl::nld',
  'xkb:no::nor', 'xkb:pl::pol', 'xkb:pt::por', 'xkb:ro::rum', 'xkb:rs::srp',
  'xkb:ru::rus', 'xkb:ru:phonetic:rus', 'xkb:se::swe', 'xkb:si::slv',
  'xkb:sk::slo', 'xkb:tr::tur', 'xkb:ua::ukr', 'xkb:us::eng',
  'xkb:us:altgr-intl:eng', 'xkb:us:dvorak:eng',
)

number_of_files_total = 0


def AddComputedActions(actions):
  """Add computed actions to the actions list.

  Arguments:
    actions: set of actions to add to.
  """

  # Actions for back_forward_menu_model.cc.
  for dir in ('BackMenu_', 'ForwardMenu_'):
    actions.add(dir + 'ShowFullHistory')
    actions.add(dir + 'Popup')
    for i in range(1, 20):
      actions.add(dir + 'HistoryClick' + str(i))
      actions.add(dir + 'ChapterClick' + str(i))

  # Actions for new_tab_ui.cc.
  for i in range(1, 10):
    actions.add('MostVisited%d' % i)

  # Actions for safe_browsing_blocking_page.cc.
  for interstitial in ('Phishing', 'Malware', 'Multiple'):
    for action in ('Show', 'Proceed', 'DontProceed'):
      actions.add('SBInterstitial%s%s' % (interstitial, action))

  # Actions for language_options_handler.cc (Chrome OS specific).
  for input_method_id in INPUT_METHOD_IDS:
    actions.add('LanguageOptions_DisableInputMethod_%s' % input_method_id)
    actions.add('LanguageOptions_EnableInputMethod_%s' % input_method_id)
    actions.add('InputMethodOptions_Open_%s' % input_method_id)
  for language_code in LANGUAGE_CODES:
    actions.add('LanguageOptions_UiLanguageChange_%s' % language_code)

def AddWebKitEditorActions(actions):
  """Add editor actions from editor_client_impl.cc.

  Arguments:
    actions: set of actions to add to.
  """
  action_re = re.compile(r'''\{ [\w']+, +\w+, +"(.*)" +\},''')

  editor_file = os.path.join(path_utils.ScriptDir(), '..', '..', 'webkit',
                             'api', 'src','EditorClientImpl.cc')
  for line in open(editor_file):
    match = action_re.search(line)
    if match:  # Plain call to RecordAction
      actions.add(match.group(1))

def GrepForActions(path, actions):
  """Grep a source file for calls to UserMetrics functions.

  Arguments:
    path: path to the file
    actions: set of actions to add to
  """
  global number_of_files_total
  number_of_files_total = number_of_files_total + 1
  # we look for the UserMetricsAction structur constructor
  # this should be on one line
  action_re = re.compile(r'UserMetricsAction\("([^"]*)')
  computed_action_re = re.compile(r'UserMetrics::RecordComputedAction')
  line_number = 0
  for line in open(path):
    line_number = line_number + 1
    match = action_re.search(line)
    if match:  # Plain call to RecordAction
      actions.add(match.group(1))
    elif computed_action_re.search(line):
      # Warn if this file shouldn't be calling RecordComputedAction.
      if os.path.basename(path) not in KNOWN_COMPUTED_USERS:
        print >>sys.stderr, 'WARNING: {0} has RecordComputedAction at {1}'.\
                             format(path, line_number)

def WalkDirectory(root_path, actions):
  for path, dirs, files in os.walk(root_path):
    if '.svn' in dirs:
      dirs.remove('.svn')
    for file in files:
      ext = os.path.splitext(file)[1]
      if ext in ('.cc', '.mm', '.c', '.m'):
        GrepForActions(os.path.join(path, file), actions)

def main(argv):
  if '--hash' in argv:
    hash_output = True
  else:
    hash_output = False
    print >>sys.stderr, "WARNING: if you added new UMA tags, you need to" + \
           " override the chromeactions.txt file and use the --hash option"
  # if we do a hash output, we want to only append NEW actions, and we know
  # the file we want to work on
  actions = set()

  if hash_output:
    f = open("chromeactions.txt")
    for line in f:
      part = line.rpartition("\t")
      part = part[2].strip()
      actions.add(part)
    f.close()


  AddComputedActions(actions)
  # TODO(fmantek): bring back webkit editor actions.
  # AddWebKitEditorActions(actions)

  # Walk the source tree to process all .cc files.
  chrome_root = os.path.join(path_utils.ScriptDir(), '..')
  WalkDirectory(chrome_root, actions)
  webkit_root = os.path.join(path_utils.ScriptDir(), '..', '..', 'webkit')
  WalkDirectory(os.path.join(webkit_root, 'glue'), actions)
  WalkDirectory(os.path.join(webkit_root, 'port'), actions)

  # print "Scanned {0} number of files".format(number_of_files_total)
  # print "Found {0} entries".format(len(actions))

  if hash_output:
    f = open("chromeactions.txt", "w")


  # Print out the actions as a sorted list.
  for action in sorted(actions):
    if hash_output:
      hash = hashlib.md5()
      hash.update(action)
      print >>f, '0x%s\t%s' % (hash.hexdigest()[:16], action)
    else:
      print action

  if hash_output:
    print "Done. Do not forget to add chromeactions.txt to your changelist"

if '__main__' == __name__:
  main(sys.argv)
