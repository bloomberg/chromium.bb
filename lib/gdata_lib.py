#!/usr/bin/python

# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Library for interacting with gdata (i.e. Google Docs, Tracker, etc)."""

import functools
import getpass
import os
import pickle
import re
import urllib
import xml.dom.minidom

# pylint: disable=W0404
import gdata.projecthosting.client
import gdata.service
import gdata.spreadsheet.service

from chromite.lib import operation

# pylint: disable=W0201,E0203

TOKEN_FILE = os.path.join(os.environ['HOME'], '.gdata_token')
CRED_FILE = os.path.join(os.environ['HOME'], '.gdata_cred.txt')

oper = operation.Operation('gdata_lib')

_BAD_COL_CHARS_REGEX = re.compile(r'[ /]')
def PrepColNameForSS(col):
  """Translate a column name for spreadsheet interface."""
  # Spreadsheet interface requires column names to be
  # all lowercase and with no spaces or other special characters.
  return _BAD_COL_CHARS_REGEX.sub('', col.lower())


# TODO(mtennant): Rename PrepRowValuesForSS
def PrepRowForSS(row):
  """Make sure spreadsheet handles all values in row as strings."""
  return dict((key, PrepValForSS(val)) for key, val in row.items())


# Regex to detect values that the spreadsheet will auto-format as numbers.
_NUM_REGEX = re.compile(r'^[\d\.]+$')
def PrepValForSS(val):
  """Make sure spreadsheet handles this value as a string."""
  if val and _NUM_REGEX.match(val):
    return "'" + val
  return val


def ScrubValFromSS(val):
  """Remove string indicator prefix if found."""
  if val and val[0] == "'":
    return val[1:]
  return val


class Creds(object):
  """Class to manage user/password credentials."""

  __slots__ = (
    'docs_auth_token',    # Docs Client auth token string
    'creds_dirty',        # True if user/password set and not, yet, saved
    'password',           # User password
    'token_dirty',        # True if auth token(s) set and not, yet, saved
    'tracker_auth_token', # Tracker Client auth token string
    'user',               # User account (foo@chromium.org)
    )

  SAVED_TOKEN_ATTRS = ('docs_auth_token', 'tracker_auth_token', 'user')

  def __init__(self):
    self.user = None
    self.password = None

    self.docs_auth_token = None
    self.tracker_auth_token = None

    self.token_dirty = False
    self.creds_dirty = False

  def SetDocsAuthToken(self, auth_token):
    """Set the Docs auth_token string."""
    self.docs_auth_token = auth_token
    self.token_dirty = True

  def SetTrackerAuthToken(self, auth_token):
    """Set the Tracker auth_token string."""
    self.tracker_auth_token = auth_token
    self.token_dirty = True

  def LoadAuthToken(self, filepath):
    """Load previously saved auth token(s) from |filepath|.

    This first clears both docs_auth_token and tracker_auth_token.
    """
    self.docs_auth_token = None
    self.tracker_auth_token = None
    try:
      f = open(filepath, 'r')
      obj = pickle.load(f)
      f.close()
      if obj.has_key('auth_token'):
        # Backwards compatability.  Default 'auth_token' is what
        # docs_auth_token used to be saved as.
        self.docs_auth_token = obj['auth_token']
        self.token_dirty = True
      for attr in self.SAVED_TOKEN_ATTRS:
        if obj.has_key(attr):
          setattr(self, attr, obj[attr])
      oper.Notice('Loaded Docs/Tracker auth token(s) from "%s"' % filepath)
    except IOError:
      oper.Error('Unable to load auth token file at "%s"' % filepath)

  def StoreAuthTokenIfNeeded(self, filepath):
    """Store auth token(s) to |filepath| if anything changed."""
    if self.token_dirty:
      self.StoreAuthToken(filepath)

  def StoreAuthToken(self, filepath):
    """Store auth token(s) to |filepath|."""
    obj = {}

    for attr in self.SAVED_TOKEN_ATTRS:
      val = getattr(self, attr)
      if val:
        obj[attr] = val

    try:
      oper.Notice('Storing Docs and/or Tracker auth token to "%s"' % filepath)
      f = open(filepath, 'w')
      pickle.dump(obj, f)
      f.close()

      self.token_dirty = False
    except IOError:
      oper.Error('Unable to store auth token to file at "%s"' % filepath)

  def SetCreds(self, user, password=None):
    if not user.endswith('@chromium.org'):
      user = '%s@chromium.org' % user

    if not password:
      password = getpass.getpass('Tracker password for %s:' % user)

    self.user = user
    self.password = password
    self.creds_dirty = True

  def LoadCreds(self, filepath):
    """Load email/password credentials from |filepath|."""
    # Read email from first line and password from second.

    with open(filepath, 'r') as f:
      (self.user, self.password) = (l.strip() for l in f.readlines())
    oper.Notice('Loaded Docs/Tracker login credentials from "%s"' % filepath)

  def StoreCredsIfNeeded(self, filepath):
    """Store email/password credentials to |filepath| if anything changed."""
    if self.creds_dirty:
      self.StoreCreds(filepath)

  def StoreCreds(self, filepath):
    """Store email/password credentials to |filepath|."""
    oper.Notice('Storing Docs/Tracker login credentials to "%s"' % filepath)
    # Simply write email on first line and password on second.
    with open(filepath, 'w') as f:
      f.write(self.user + '\n')
      f.write(self.password + '\n')

    self.creds_dirty = False


class IssueComment(object):
  """Represent a Tracker issue comment."""

  __slots__ = ['title', 'text']

  def __init__(self, title, text):
    self.title = title
    self.text = text

  def __str__(self):
    text = '<no comment>'
    if self.text:
      text = '\n  '.join(self.text.split('\n'))
    return '%s:\n  %s' % (self.title, text)


class Issue(object):
  """Represents one Tracker Issue."""

  SlotDefaults = {
    'comments': [], # List of IssueComment objects
    'id': 0,        # Issue id number (int)
    'labels': [],   # List of text labels
    'owner': None,  # Current owner (text, chromium.org account)
    'status': None, # Current issue status (text) (e.g. Assigned)
    'summary': None,# Issue summary (first comment)
    'title': None,  # Title text
    }

  __slots__ = SlotDefaults.keys()

  def __init__(self, **kwargs):
    """Init for one Issue object.

    |kwargs| - key/value arguments to give initial values to
    any additional attributes on |self|.
    """
    # Use SlotDefaults overwritten by kwargs for starting slot values.
    slotvals = self.SlotDefaults.copy()
    slotvals.update(kwargs)
    for slot in self.__slots__:
      setattr(self, slot, slotvals.pop(slot))
    if slotvals:
      raise ValueError('I do not know what to do with %r' % slotvals)

  def __str__(self):
    """Pretty print of issue."""
    lines = ['Issue %d - %s' % (self.id, self.title),
             'Status: %s, Owner: %s' % (self.status, self.owner),
             'Labels: %s' % ', '.join(self.labels),
             ]

    if self.summary:
      lines.append('Summary: %s' % self.summary)

    if self.comments:
      lines.extend(self.comments)

    return '\n'.join(lines)

  def InitFromTracker(self, t_issue, project_name):
    """Initialize |self| from tracker issue |t_issue|"""

    self.id = int(t_issue.id.text.split('/')[-1])
    self.labels = [label.text for label in t_issue.label]
    if t_issue.owner:
      self.owner = t_issue.owner.username.text
    self.status = t_issue.status.text
    self.summary = t_issue.content.text
    self.title = t_issue.title.text
    self.comments = self.GetTrackerIssueComments(self.id, project_name)

  def GetTrackerIssueComments(self, issue_id, project_name):
    """Retrieve comments for |issue_id| from comments URL"""
    comments = []

    feeds = 'http://code.google.com/feeds'
    url = '%s/issues/p/%s/issues/%d/comments/full' % (feeds, project_name,
                                                      issue_id)
    doc = xml.dom.minidom.parse(urllib.urlopen(url))
    entries = doc.getElementsByTagName('entry')
    for entry in entries:
      title_text_list = []
      for key in ('title', 'content'):
        child = entry.getElementsByTagName(key)[0].firstChild
        title_text_list.append(child.nodeValue if child else None)
      comments.append(IssueComment(*title_text_list))

    return comments


class TrackerComm(object):
  """Class to manage communication with Tracker."""

  __slots__ = (
    'author',       # Author when creating/editing Tracker issues
    'it_client',    # Issue Tracker client
    'project_name', # Tracker project name
    )

  def __init__(self):
    self.author = None
    self.it_client = None
    self.project_name = None

  def Connect(self, creds, project_name, source='chromiumos'):
    self.project_name = project_name

    it_client = gdata.projecthosting.client.ProjectHostingClient()
    it_client.source = source

    if creds.tracker_auth_token:
      oper.Notice('Logging into Tracker using previous auth token.')
      it_client.auth_token = gdata.gauth.ClientLoginToken(
        creds.tracker_auth_token)
    else:
      oper.Notice('Logging into Tracker as "%s".' % creds.user)
      it_client.ClientLogin(creds.user, creds.password,
                            source=source, service='code',
                            account_type='GOOGLE')
      creds.SetTrackerAuthToken(it_client.auth_token.token_string)

    self.author = creds.user
    self.it_client = it_client

  # TODO(mtennant): This method works today, but is not being actively used.
  # Leaving it in, because a logical use of the method is for to verify
  # that a Tracker issue in the package spreadsheet is open, and to add
  # comments to it when new upstream versions become available.
  def GetTrackerIssueById(self, tid):
    """Get tracker issue given |tid| number.  Return Issue object if found."""

    query = gdata.projecthosting.client.Query(issue_id=str(tid))

    try:
      feed = self.it_client.get_issues(self.project_name, query=query)
    except gdata.client.RequestError:
      return None

    if feed.entry:
      issue = Issue()
      issue.InitFromTracker(feed.entry[0], self.project_name)
      return issue
    return None

  def CreateTrackerIssue(self, issue):
    """Create a new issue in Tracker according to |issue|."""
    created = self.it_client.add_issue(project_name=self.project_name,
                                       title=issue.title,
                                       content=issue.summary,
                                       author=self.author,
                                       status=issue.status,
                                       owner=issue.owner,
                                       labels=issue.labels)
    issue.id = int(created.id.text.split('/')[-1])
    return issue.id

  def AppendTrackerIssueById(self, issue_id, comment):
    """Append |comment| to issue |issue_id| in Tracker"""
    self.it_client.update_issue(project_name=self.project_name,
                                issue_id=issue_id,
                                author=self.author,
                                comment=comment)
    return issue_id


class SpreadsheetRow(dict):
  """Minor semi-immutable extension of dict to keep the original spreadsheet
  row object and spreadsheet row number as attributes.

  No changes are made to equality checking or anything else, so client code
  that wishes to handle this as a pure dict can.
  """

  def __init__(self, ss_row_obj, ss_row_num, mapping=None):
    if mapping:
      dict.__init__(self, mapping)

    self.ss_row_obj = ss_row_obj
    self.ss_row_num = ss_row_num

  def __setitem__(self, key, val):
    raise TypeError('setting item in SpreadsheetRow not supported')

  def __delitem__(self, key):
    raise TypeError('deleting item in SpreadsheetRow not supported')


class SpreadsheetError(RuntimeError):
  """Error class for spreadsheet communication errors."""

def ReadWriteDecorator(func):
  """Raise SpreadsheetError if appropriate."""
  def f(self, *args, **kwargs):
    try:
      return func(self, *args, **kwargs)
    except gdata.service.RequestError as ex:
      raise SpreadsheetError(str(ex))

  f.__name__ = func.__name__
  return f

class SpreadsheetComm(object):
  """Class to manage communication with one Google Spreadsheet worksheet."""

  # Row numbering in spreadsheets effectively starts at 2 because row 1
  # has the column headers.
  ROW_NUMBER_OFFSET = 2

  # Spreadsheet column numbers start at 1.
  COLUMN_NUMBER_OFFSET = 1

  __slots__ = (
    '_columns',    # Tuple of translated column names, filled in as needed
    '_rows',       # Tuple of Row dicts in order, filled in as needed
    'gd_client',   # Google Data client
    'ss_key',      # Spreadsheet key
    'ws_name',     # Worksheet name
    'ws_key',      # Worksheet key
    )

  @property
  def columns(self):
    """The columns property is filled in on demand.

    It is a tuple of column names, each run through PrepColNameForSS.
    """
    if self._columns is None:
      query = gdata.spreadsheet.service.CellQuery()
      query['max-row'] = '1'
      feed = self.gd_client.GetCellsFeed(self.ss_key, self.ws_key, query=query)

      # The use of PrepColNameForSS here looks weird, but the values
      # in row 1 are the unaltered column names, rather than the restricted
      # column names used for interface purposes.  In other words, if the
      # spreadsheet looks like it has a column called "Foo Bar", then the
      # first row will have a value "Foo Bar" but all interaction with that
      # column for other rows will use column key "foobar".  Translate to
      # restricted names now with PrepColNameForSS.
      cols = [PrepColNameForSS(entry.content.text) for entry in feed.entry]

      self._columns = tuple(cols)

    return self._columns

  @property
  def rows(self):
    """The rows property is filled in on demand.

    It is a tuple of SpreadsheetRow objects.
    """
    if self._rows is None:
      rows = []

      feed = self.gd_client.GetListFeed(self.ss_key, self.ws_key)
      for rowIx, rowObj in enumerate(feed.entry, start=self.ROW_NUMBER_OFFSET):
        row_dict = dict((key, ScrubValFromSS(val.text))
                        for key, val in rowObj.custom.iteritems())
        rows.append(SpreadsheetRow(rowObj, rowIx, row_dict))

      self._rows = tuple(rows)

    return self._rows

  def __init__(self):
    for slot in self.__slots__:
      setattr(self, slot, None)

  def Connect(self, creds, ss_key, ws_name, source='chromiumos'):
    """Login to spreadsheet service and set current worksheet.

    |creds| Credentials object for Google Docs
    |ss_key| Spreadsheet key
    |ws_name| Worksheet name
    |source| Name to associate with connecting service
    """
    self._Login(creds, source)
    self.SetCurrentWorksheet(ws_name, ss_key=ss_key)

  def SetCurrentWorksheet(self, ws_name, ss_key=None):
    """Change the current worksheet.  This clears all caches."""
    if ss_key and ss_key != self.ss_key:
      self.ss_key = ss_key
      self._ClearCache()

    self.ws_name = ws_name

    ws_key = self._GetWorksheetKey(self.ss_key, self.ws_name)
    if ws_key != self.ws_key:
      self.ws_key = ws_key
      self._ClearCache()

  def _ClearCache(self, keep_columns=False):
    """Called whenever column/row data might be stale."""
    self._rows = None
    if not keep_columns:
      self._columns = None

  def _Login(self, creds, source):
    """Login to Google doc client using given |creds|."""
    gd_client = RetrySpreadsheetsService()
    gd_client.source = source

    # Login using previous auth token if available, otherwise
    # use email/password from creds.
    if creds.docs_auth_token:
      oper.Notice('Logging into Docs using previous auth token.')
      gd_client.SetClientLoginToken(creds.docs_auth_token)
    else:
      oper.Notice('Logging into Docs as "%s".' % creds.user)
      gd_client.email = creds.user
      gd_client.password = creds.password
      gd_client.ProgrammaticLogin()
      creds.SetDocsAuthToken(gd_client.GetClientLoginToken())

    self.gd_client = gd_client

  def _GetWorksheetKey(self, ss_key, ws_name):
    """Get the worksheet key with name |ws_name| in spreadsheet |ss_key|."""
    feed = self.gd_client.GetWorksheetsFeed(ss_key)
    # The worksheet key is the last component in the URL (after last '/')
    for entry in feed.entry:
      if ws_name == entry.title.text:
        return entry.id.text.split('/')[-1]

    oper.Die('Unable to find worksheet "%s" in spreadsheet "%s"' %
             (ws_name, ss_key))

  @ReadWriteDecorator
  def GetColumns(self):
    """Return tuple of column names in worksheet.

    Note that each returned name has been run through PrepColNameForSS.
    """
    return self.columns

  @ReadWriteDecorator
  def GetColumnIndex(self, colName):
    """Get the column index (starting at 1) for column |colName|"""
    try:
      # Spreadsheet column indices start at 1, so +1.
      return self.columns.index(colName) + self.COLUMN_NUMBER_OFFSET
    except ValueError:
      return None

  @ReadWriteDecorator
  def GetRows(self):
    """Return tuple of SpreadsheetRow objects in order."""
    return self.rows

  @ReadWriteDecorator
  def GetRowCacheByCol(self, column):
    """Return a dict for looking up rows by value in |column|.

    Each row value is a SpreadsheetRow object.
    If more than one row has the same value for |column|, then the
    row objects will be in a list in the returned dict.
    """
    row_cache = {}

    for row in self.GetRows():
      col_val = row[column]

      current_entry = row_cache.get(col_val, None)
      if current_entry and type(current_entry) is list:
        current_entry.append(row)
      elif current_entry:
        current_entry = [current_entry, row]
      else:
        current_entry = row

      row_cache[col_val] = current_entry

    return row_cache

  @ReadWriteDecorator
  def InsertRow(self, row):
    """Insert |row| at end of spreadsheet."""
    self.gd_client.InsertRow(row, self.ss_key, self.ws_key)
    self._ClearCache(keep_columns=True)

  @ReadWriteDecorator
  def UpdateRowCellByCell(self, rowIx, row):
    """Replace cell values in row at |rowIx| with those in |row| dict."""
    for colName in row:
      colIx = self.GetColumnIndex(colName)
      if colIx is not None:
        self.ReplaceCellValue(rowIx, colIx, row[colName])
    self._ClearCache(keep_columns=True)

  @ReadWriteDecorator
  def DeleteRow(self, ss_row):
    """Delete the given |ss_row| (must be original spreadsheet row object."""
    self.gd_client.DeleteRow(ss_row)
    self._ClearCache(keep_columns=True)

  @ReadWriteDecorator
  def ReplaceCellValue(self, rowIx, colIx, val):
    """Replace cell value at |rowIx| and |colIx| with |val|"""
    self.gd_client.UpdateCell(rowIx, colIx, val, self.ss_key, self.ws_key)
    self._ClearCache(keep_columns=True)

  @ReadWriteDecorator
  def ClearCellValue(self, rowIx, colIx):
    """Clear cell value at |rowIx| and |colIx|"""
    self.ReplaceCellValue(rowIx, colIx, None)


class RetrySpreadsheetsService(gdata.spreadsheet.service.SpreadsheetsService):
  """Extend SpreadsheetsService to put retry logic around http request method.

  The entire purpose of this class is to remove some flakiness from
  interactions with Google Docs spreadsheet service, in the form of
  certain 40* http error responses to http requests.  This is documented in
  http://code.google.com/p/chromium-os/issues/detail?id=23819.
  There are two "request" methods that need to be wrapped in retry logic.
  1) The request method on self.  Original implementation is in
     base class atom.service.AtomService.
  2) The request method on self.http_client.  The class of self.http_client
     can actually vary, so the original implementation of the request
     method can also vary.
  """
  # pylint: disable=R0904

  TRY_MAX = 5
  RETRYABLE_STATUSES = (403,)

  def __init__(self, *args, **kwargs):
    gdata.spreadsheet.service.SpreadsheetsService.__init__(self, *args,
                                                           **kwargs)

    # Wrap self.http_client.request with retry wrapper.  This request method
    # is used by ProgrammaticLogin(), at least.
    if hasattr(self, 'http_client'):
      self.http_client.request = functools.partial(self._RetryRequest,
                                                   self.http_client.request)

    self.request = functools.partial(self._RetryRequest, self.request)

  def _RetryRequest(self, func, *args, **kwargs):
    """Retry wrapper for bound |func|, passing |args| and |kwargs|.

    This retry wrapper can be used for any http request |func| that provides
    an http status code via the .status attribute of the returned value.

    Retry when the status value on the return object is in RETRYABLE_STATUSES,
    and run up to TRY_MAX times.  If successful (whether or not retries
    were necessary) return the last return value returned from base method.
    If unsuccessful return the first return value returned from base method.
    """
    first_retval = None
    for try_ix in xrange(1, self.TRY_MAX + 1):
      retval = func(*args, **kwargs)
      if retval.status not in self.RETRYABLE_STATUSES:
        return retval
      else:
        oper.Warning('Retry-able HTTP request failure (status=%d), try %d/%d' %
                     (retval.status, try_ix, self.TRY_MAX))
        if not first_retval:
          first_retval = retval

    oper.Warning('Giving up on HTTP request after %d tries' % self.TRY_MAX)
    return first_retval


# Support having this module test itself if run as __main__, by leveraging
# the gdata_lib_unittest module.
# Also, the unittests serve as extra documentation.
if __name__ == '__main__':
  import gdata_lib_unittest
  gdata_lib_unittest.unittest.main(gdata_lib_unittest)
