# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import re

from google.appengine.api import datastore_errors
from google.appengine.ext import db
from google.appengine.ext import webapp
from google.appengine.ext.webapp import template
from google.appengine.api import users

import model

# Could replace this with a function if there is ever any reason
# to spread entries over multiple datastores.  Consistency is only
# gauranteed within a datastore, but access should be limited to
# about 1 per second.  That should not be a problem for us.
DATASTORE_KEY = db.Key.from_path('Stats', 'default')


class MainPage(webapp.RequestHandler):
  """Provide interface for interacting with DB."""

  # Regex to peel SQL-like SELECT off front, if present, grabbing SELECT args.
  # Example: "SELECT foo,bar WHERE blah blah"
  #          ==> group(1)="foo,bar", group(2)="WHERE blah blah"
  # Example: "SELECT foo , bar"
  #          ==> group(1)="foo , bar", group(2)=""
  # Example: "WHERE blah blah"
  #          ==> No match
  QUERY_SELECT_PREFIX_RE = re.compile(r'^\s*SELECT\s+'
                                      r'([^\s,]+(?:\s*,\s*[^\s,]+)*)' # Group 1
                                      r'(?:$|\s+)(.*)',               # Group 2
                                      re.IGNORECASE | re.VERBOSE)

  # Regex to determine if WHERE is present, and capture everything after it.
  # Example: "WHERE foo=bar ORDER BY whatever"
  #          ==> group(1)="foo=bar ORDER BY whatever"
  # Example: "ORDER BY whatever"
  #          ==> No match
  QUERY_WHERE_PREFIX_RE = re.compile(r'^WHERE\s+(.+)$',
                                     re.IGNORECASE | re.VERBOSE)

  # Regex to discover ORDER BY columns in order to highlight them in results.
  QUERY_ORDER_RE = re.compile(r'ORDER\s+BY\s+(\S+)', re.IGNORECASE)

  # Regex to discover LIMIT value in query.
  QUERY_LIMIT_RE = re.compile(r'LIMIT\s+(\d+)', re.IGNORECASE)

  # Regex for separating tokens by commas, allowing spaces on either side.
  COMMA_RE = re.compile(r'\s*,\s*')

  # Default columns to show in results table if no SELECT given.
  DEFAULT_COLUMNS = ['end_date', 'cmd_line', 'run_time', 'board',
                     'package_count']

  # All possible columns in Statistics model.
  ALL_COLUMNS = sorted(model.Statistics.properties())

  # Provide example queries in interface as a form of documentation.
  EXAMPLE_QUERIES = [
    ("ORDER BY end_date,run_time"
     " LIMIT 30"),
    ("WHERE username='mtennant'"
     " ORDER BY end_date DESC"
     " LIMIT 30"),
    ("SELECT end_datetime,cmd_base,cmd_args,run_time,package_count"
     " WHERE board='amd64-generic'"
     " ORDER BY end_datetime"
     " LIMIT 30"),
    ("SELECT end_date,cmd_base,run_time,board,package_count"
     " WHERE end_date=DATE('2012-03-28')"
     " ORDER BY run_time"
     " LIMIT 30"),
    ("SELECT end_date,cmd_base,cmd_args,run_time,username"
     " WHERE run_time>20"
     " LIMIT 30"),
    ]

  def get(self):
    """Support GET to stats page."""
    # Note that google.com authorization is required to access this page, which
    # is controlled in app.yaml and on appspot admin page.

    orig_query = self.request.get('query')
    logging.debug('Received raw query %r', orig_query)

    # If no LIMIT was provided, default to a LIMIT of 30 for sanity.
    if not self.QUERY_LIMIT_RE.search(orig_query):
      orig_query += ' LIMIT 30'

    query = orig_query

    # Peel off "SELECT" clause from front of query.  GCL does not support SELECT
    # filtering, but we will support it right here to select/filter columns.
    query, columns = self._RemoveSelectFromQuery(query)
    if query == orig_query and columns == self.DEFAULT_COLUMNS:
      # This means there was no SELECT in query.  That is equivalent to
      # SELECT of default columns, so show that to user.
      orig_query = 'SELECT %s %s' % (','.join(columns), orig_query)

    # All queries should have the "ancestor" WHERE clause in them, but that
    # need not be exposed to interface.  Insert the clause intelligently.
    query = self._AdjustWhereInQuery(query)

    # If results are sorted by any column that column should be in bold.
    bold_columns = None
    match = self.QUERY_ORDER_RE.search(query)
    if match:
      bold_columns = set(self.COMMA_RE.split(match.group(1)))

    stat_entries = []
    error_msg = None
    try:
      stat_entries = model.Statistics.gql(query, DATASTORE_KEY)
    except datastore_errors.BadQueryError, ex:
      error_msg = '<p>%s.</p><p>Actual GCL query used: "%s"</p>' % (ex, query)

    results_html_table = self._PrepareResultsTable(stat_entries, columns,
                                                   bold_columns=bold_columns)

    column_html_list = self._PrepareColumnList(self.ALL_COLUMNS,
                                               self.DEFAULT_COLUMNS)

    template_values = {
      'column_list': column_html_list,
      'error_msg': error_msg,
      'example_queries': self.EXAMPLE_QUERIES,
      'gcl_query': query,
      'results_table': results_html_table,
      'user_email': users.get_current_user(),
      'user_query': orig_query,
      }

    path = os.path.join(os.path.dirname(__file__), 'index.html')
    self.response.out.write(template.render(path, template_values))

  def _RemoveSelectFromQuery(self, query):
    """Remove SELECT clause from |query|, return tuple (new_query, columns)."""
    match = self.QUERY_SELECT_PREFIX_RE.search(query)
    if match:
      # A SELECT clause is present.  Remove it but save requested columns.
      columns = self.COMMA_RE.split(match.group(1))
      query = match.group(2)

      if columns == ['*']:
        columns = self.ALL_COLUMNS

      logging.debug('Columns selected for viewing: %s', ', '.join(columns))
      return query, columns
    else:
      logging.debug('Using default columns for viewing: %s',
                    ', '.join(self.DEFAULT_COLUMNS))
      return query, self.DEFAULT_COLUMNS

  def _AdjustWhereInQuery(self, query):
    """Insert WHERE ANCESTOR into |query| and return."""
    match = self.QUERY_WHERE_PREFIX_RE.search(query)
    if match:
      return 'WHERE ANCESTOR IS :1 AND %s' % match.group(1)
    else:
      return 'WHERE ANCESTOR IS :1 %s' % query

  def _PrepareResultsTable(self, stat_entries, columns, bold_columns):
    """Prepare html table for |stat_entries| using only |columns|."""
    # One header blank for row numbers, then each column name.
    headers = ('<tr>\n<td></td>\n%s\n</tr>' %
               '\n'.join(['<td>%s</td>' % c for c in columns]))

    # Prepare list of table rows, one for each stat entry.
    rows = []
    for stat_ix, stat_entry in enumerate(stat_entries):
      row_data = ['<td>%d</td>' % (stat_ix + 1)]
      for col in columns:
        datum = getattr(stat_entry, col)
        if bold_columns and col in bold_columns:
          datum = '<b>%s</b>' % datum
        row_data.append('<td>%s</td>' % datum)

      rows.append('<tr>%s</tr>' % '\n'.join(row_data))

    return headers + '\n' + '\n'.join(rows)

  def _PrepareColumnList(self, columns, default_columns):
    """Prepare html list for |columns| with each |default_columns| bolded."""
    default_columns = set(default_columns)

    items = []
    for col in columns:
      if col in default_columns:
        col = '<b>%s</b>' % col

      items.append('<li>%s</li>' % col)

    return '\n'.join(items)


class PostPage(webapp.RequestHandler):
  """Provides interface for uploading command stats to database."""

  NO_VALUE = '__NO_VALUE_AT_ALL__'

  def post(self):
    """Support POST of command stats."""
    logging.info('Stats POST received at %r', self.request.uri)

    new_stat = model.Statistics(parent=DATASTORE_KEY)

    # Check each supported DB property to see if it has a value set
    # in the POST request.
    for prop in model.Statistics.properties():
      # Skip properties with auto_now or auto_now_add enabled.
      model_prop = getattr(model.Statistics, prop)
      if ((hasattr(model_prop, 'auto_now_add') and model_prop.auto_now_add) or
          (hasattr(model_prop, 'auto_now') and model_prop.auto_now)):
        continue

      # Note that using hasattr with self.request does not work at all.
      # It (almost) always says the attribute is not present, when getattr
      # does actually return a value.  Also note that self.request.get is
      # not returning None as the default value if no explicit default value
      # is provided, contrary to the spec for dict.get.
      value = self.request.get(prop, self.NO_VALUE)

      if value is not self.NO_VALUE:
        # String properties must be 500 characters or less (GQL requirement).
        if isinstance(model_prop, db.StringProperty) and len(value) > 500:
          logging.debug('  String property %r too long.  Cutting off at 500'
                        ' characters.', prop)
          value = value[:500]

        # Integer properties require casting
        if isinstance(model_prop, db.IntegerProperty):
          value = int(value)

        logging.debug('  Stats POST property %r ==> %r', prop, value)
        setattr(new_stat, prop, value)

    # Use automatically set end_datetime prop to set end_date and end_time.
    new_stat.end_time = new_stat.end_datetime.time()
    new_stat.end_date = new_stat.end_datetime.date()

    # Save to model.
    new_stat.put()
