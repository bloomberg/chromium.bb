# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Python representation for Chromium Preferences.

Obtain one of these from a call to PyUITest::GetPrefsInfo() or
PyUITest::GetLocalStatePrefsInfo().

Example:
class MyTest(pyauto.PyUITest):
  def testBasic(self):
    info = self.GetPrefsInfo()  # fetch prefs snapshot
    print info.Prefs()  # all prefs
    print info.Prefs('session.restore_on_startup')  # a single pref

See more tests in chrome/test/functional/prefs.py.
"""

import simplejson as json

from pyauto_errors import JSONInterfaceError


class PrefsInfo(object):
  """Represent info for Chromium preferences.

  The info is represented as a hierarchy of prefs values.
  The values could be plain (integer, bool, float) or complex (like
  dictionary, list).
  """
  def __init__(self, json_string):
    """Initialize a PrefsInfo from a json string.

    Args:
      json_string: a json string, as returned by a json ipc call for the
                   command 'GetPrefsInfo'
                   A typical string representing prefs snapshot looks like:
                   { u'prefs':
                     { u'alternate_error_pages': {u'enabled': True},
                       u'autofill': { u'auxiliary_profiles_enabled': False,
                                      u'default_creditcard': u'',
                                      u'default_profile': u'',
                                      u'enabled': True,
                                      u'infobar_shown': False,
                                      u'negative_upload_rate': 0.01,
                                      u'positive_upload_rate': 0.01},
                       u'bookmark_bar': {u'show_on_all_tabs': False},
                       ...
                       ...
                     }
                   }

    Raises:
      pyauto_errors.JSONInterfaceError if the automation call returns an error.
    """
    # JSON string prepared in PrefsInfo() in automation_provider.cc
    self.prefsdict = json.loads(json_string)
    if self.prefsdict.has_key('error'):
      raise JSONInterfaceError(self.prefsdict['error'])

  def Prefs(self, path=None):
    """Get preferences.

    The preference dictionary (when using path=None) looks like:

      { u'alternate_error_pages': {u'enabled': True},
        u'autofill': { u'auxiliary_profiles_enabled': False,
                       u'default_creditcard': u'',
                       u'default_profile': u'',
                       u'enabled': True,
                       u'infobar_shown': False,
                       u'negative_upload_rate': 0.01,
                       u'positive_upload_rate': 0.01},
        u'bookmark_bar': {u'show_on_all_tabs': False},
        ...
        ...
      }

    In this case, to fetch the preference value for autofill enabled, use
    'autofill.enabled' as the path.

    Args:
      path: If specified, return the preference item for the given path.
            path is a dot-separated string like "session.restore_on_startup".
            One of the equivalent names in chrome/common/pref_names.h could
            also be used.

    Returns:
      preference value. It could be a dictionary (like the example above), a
          list or a plain value.
      None, if prefernece for path not found (if path is given).
    """
    all = self.prefsdict.get('prefs', {})
    if not path:  # No path given. Return all prefs.
      return all
    for part in path.split('.'):  # Narrow down to the requested prefs path.
      all = all.get(part)
      if all is None: return None
    return all
