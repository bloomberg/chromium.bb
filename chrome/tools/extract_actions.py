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
