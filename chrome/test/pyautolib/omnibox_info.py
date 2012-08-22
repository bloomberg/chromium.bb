# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Python representation for Chromium Omnibox.

Obtain one of these from PyUITestSuite::GetOmniboxInfo() call.

Example:
class MyTest(pyauto.PyUITest):
  def testBasic(self):
    info = self.OmniboxInfo()  # fetch omnibox snapshot
    print info.Matches()

See more tests in chrome/test/functional/omnibox.py.
"""

import simplejson as json

from pyauto_errors import JSONInterfaceError


class OmniboxInfo(object):
  """Represent info for Chromium Omnibox.

  Info contains:
    - a list of matches in the same order as you'd see in the omnibox,
    - a dictionary of properties related to the omnibox.

  Sample info text:

  { u'matches': [
    {
      u'contents': u'google',
      u'description': u'Google Search',
      u'destination_url': u'http://www.google.com/search?aq=f&'
                           'sourceid=chrome&ie=UTF-8&q=google',
      u'starred': False,
      u'type': u'search-what-you-typed'},
    {
      u'contents': u'maps.google.com/',
      u'description': u'Google Maps',
      u'destination_url': u'http://maps.google.com/',
      u'starred': False,
      u'type': u'navsuggest'},
    { u'contents': u'google maps',
      u'description': u'',
      u'destination_url': u'http://www.google.com/search?aq=0&oq=google&'
                           'sourceid=chrome&ie=UTF-8&q=google+maps',
      u'starred': False,
      u'type': u'search-suggest'},
    { u'contents': u'google earth',
      u'description': u'',
      u'destination_url': u'http://www.google.com/search?aq=1&oq=google&'
                           'sourceid=chrome&ie=UTF-8&q=google+earth',
      u'starred': False,
      u'type': u'search-suggest'},
    { u'contents': u'Search Google for <enter query>',
      u'description': u'(Keyword: google.com)',
      u'destination_url': u'',
      u'starred': False,
      u'type': u'search-other-engine'}],

    u'properties': { u'has_focus': True,
                     u'keyword': u'',
                     u'query_in_progress': False,
                     u'text': u'google'}}
  """
  def __init__(self, json_string):
    """Initialize a OmniboxInfo from a json string.

    Args:
      json_string: a json string, as returned by a json ipc call for the
                   command 'GetOmniboxInfo'

    Raises:
      pyauto_errors.JSONInterfaceError if the automation call returns an error.
    """
    # JSON string prepared in GetOmniboxInfo() in automation_provider.cc
    self.omniboxdict = json.loads(json_string)
    if self.omniboxdict.has_key('error'):
      raise JSONInterfaceError(self.omniboxdict['error'])

  def Matches(self):
    """Get omnibox matches.

    Returns:
      a list of omnibox match items.
    """
    return self.omniboxdict.get('matches', [])

  def MatchesWithAttributes(self, attr_dict):
    """Find all omnibox matches which match the attributes in |attr_dict|.

    Args:
      attr_dict: a dictionary of attributes to be satisfied.
                 All attributes in the given dictionary should be satisfied.
                 example:
                   { 'destiantion_url': 'http://www.google.com/',
                     'description': 'Google' }

    Returns:
      a list of omnibox match items.
    """
    out = []
    for item in self.Matches():
      matched = True
      for key, val in attr_dict.iteritems():
        if not item.has_key(key) or item[key] != val:
          matched = False
      if matched:
        out.append(item)
    return out

  def Properties(self, key=None):
    """Get the properties

    Args:
      key: if specified, value for the given property is returned.

    Returns:
      a dictionary of properties if no key is given, OR
      value corresponding to a particular property if key is given
    """
    all = self.omniboxdict.get('properties')
    if not key:
      return all
    return all.get(key)

  def Text(self):
    """Get the text in the omnibox.

    This need not be the same as the user-inputted text, since omnibox may
    autocomplete some URLs, or the user may move omnibox popup selection
    up/down.
    """
    return self.Properties('text')

  def IsQueryInProgress(self):
    """Determine if a query is in progress."""
    return self.Properties('query_in_progress')
