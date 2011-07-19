#!/usr/bin/python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be found
# in the LICENSE file.

"""Extracts registration forms from the corresponding HTML files.

Used for extracting forms within HTML files. This script is used in
conjunction with the webforms_aggregator.py script, which aggregates web pages
with fillable forms (i.e registration forms).

The purpose of this script is to extract out JavaScript elements that may be
causing parsing errors and timeout issues when running browser_tests.

Used as a standalone script but assumes that it is run from the directory in
which it is checked into.

Usage: forms_extractor.py [options]

Options:
  -l LOG_LEVEL, --log_level=LOG_LEVEL,
    LOG_LEVEL: debug, info, warning or error [default: error]
  -h, --help  show this help message and exit
"""

import glob
import logging
from optparse import OptionParser
import os
import re
import sys


class FormsExtractor(object):
  """Extracts HTML files, leaving only registration forms from the HTML file."""
  HTML_FILES_PATTERN = r'*.html'
  HTML_FILE_PREFIX = r'grabber-'
  FORM_FILE_PREFIX = r'top100_'
  REGISTRATION_PAGES_DIR = os.path.join(os.pardir, 'test', 'data', 'autofill',
                                        'heuristics', 'input')
  EXTRACTED_FORMS_DIR = os.path.join(os.pardir, 'test', 'data', 'autofill',
                                     'heuristics', 'input')

  logger = logging.getLogger(__name__)
  log_handlers = {'StreamHandler': None}

  # This pattern is used for removing all |<script>| code.
  re_script_pattern = re.compile(
      ur"""
      <script       # A new opening 'script' tag.
      \b            # The end of the word 'script'.
      .*?           # Any characters (non-greedy).
      >             # Ending of the opening tag '>'.
      .*?           # Any characters (non-greedy) between the tags.
      </script\s*>  # The '<script>' closing tag.
      """, re.U | re.S | re.I | re.X)

  # This pattern is used to remove all href js code.
  re_href_js_pattern = re.compile(
      ur"""
      \bhref             # The beginning of a new word.
      \s*=\s*            # Matches any spaces before and after the '=' symbol.
      (?P<quote>[\'\"])  # Captures the single or double quote in the
                         # matching pattern.
      \s*javascript\s*   # Matches anything before and after 'javascript'.
      .*?                # Matches any characters (non-greedy) between the
                         # quotes.
      \1                 # Matches the previously captured single or double
                         # quotes.
      """, re.U | re.S | re.I | re.X)

  # This pattern is used for capturing js event |'onmouseover="..."'|
  re_event = (
      ur"""
      \b                 # The beginning of a new word.
      on\w+?             # Matches all words starting with 'on' (non-greedy)
                         # |onmouseover|.
      \s*=\s*            # Matches any spaces before and after the '=' symbol.
      (?P<quote>[\'\"])  # Captures the single or double quote in the matching
                         # pattern.
      .*?                # Matches any characters (non-greedy) between the
                         # quotes.
      \1                 # Matches the previously captured single or double
                         # quotes.
      """)

  # This pattern is used for removing code with js events, such as |onload|.
  # By adding the leading |ur'<[^<>]*?'| and the trailing |'ur'[^<>]*?>'| the
  # pattern matches to strings such as '<tr class="nav"
  # onmouseover="mOvr1(this);" onmouseout="mOut1(this);">'
  re_tag_with_events_pattern = re.compile(
      ur"""
      <        # Matches character '<'.
      [^<>]*?  # Matches any characters except '<' and '>' (non-greedy).""" +
      re_event +
      ur"""
      [^<>]*?  # Matches any characters except '<' and '>' (non-greedy)
      >        # Matches character '>'.
      """, re.U | re.S | re.I | re.X)

  # Add white space characters at the end of the matched event. Leaves only the
  # leading white space when substituted with empty string.
  re_event_pattern = re.compile(re_event + ur'\s*', re.U | re.S | re.I | re.X)


  def __init__(self, input_dir=REGISTRATION_PAGES_DIR,
               output_dir=EXTRACTED_FORMS_DIR, logging_level=None):
    """Creates a FormsExtractor object.

    Args:
      input_dir: the directory of HTML files.
      output_dir: the directory where the registration form files will be
                  saved.
      logging_level: verbosity level, default is None.

    Raises:
      IOError exception if input directory doesn't exist.
    """
    if logging_level:
      if not self.log_handlers['StreamHandler']:
        console = logging.StreamHandler()
        console.setLevel(logging.DEBUG)
        self.log_handlers['StreamHandler'] = console
        self.logger.addHandler(console)
      self.logger.setLevel(logging_level)
    else:
      if self.log_handlers['StreamHandler']:
        self.logger.removeHandler(self.log_handlers['StreamHandler'])
        self.log_handlers['StreamHandler'] = None

    self._input_dir = input_dir
    self._output_dir = output_dir
    if not os.path.isdir(self._input_dir):
      error_msg = 'Directory "%s" doesn\'t exist.' % self._input_dir
      self.logger.error('Error: %s', error_msg)
      raise IOError(error_msg)
    if not os.path.isdir(output_dir):
      os.makedirs(output_dir)
    self._form_location_comment = ''

  def _SubstituteAllEvents(self, matchobj):
    """Remove all js events that are present as attributes within a tag.

    Args:
      matchobj: A regexp |re.MatchObject| containing text that has at least one
      event. Example: |<tr class="nav" onmouseover="mOvr1(this);"
      onmouseout="mOut1(this);">|
    Returns:
      The text containing the tag with all the attributes except for the tags
      with events. Example: |<tr class="nav">|
    """
    tag_with_all_attrs = matchobj.group(0)
    tag_with_all_attrs_except_events = self.re_event_pattern.sub(
        '', tag_with_all_attrs)
    return tag_with_all_attrs_except_events

  def Extract(self):
    """Extracts and saves the extracted registration forms.

    Iterates through all the HTML files.
    """
    pathname_pattern = os.path.join(self._input_dir, self.HTML_FILES_PATTERN)
    html_files = [f for f in glob.glob(pathname_pattern) if os.path.isfile(f)]
    for filename in html_files:
      self.logger.info('extracting file "%s" ...', filename)
      with open(filename) as f:
        html_content = self.re_tag_with_events_pattern.sub(
            self._SubstituteAllEvents,
            self.re_href_js_pattern.sub(
                '', self.re_script_pattern.sub('', f.read())))
      try:
        with open(
            os.path.join(self._output_dir, '%s%s' % (
                self.FORM_FILE_PREFIX, os.path.split(filename)[1].replace(
                    self.HTML_FILE_PREFIX, '')) ), 'wb') as f:
                  f.write(html_content)
      except IOError as e:
        self.logger.error('Error: %s', e)
        continue
      self.logger.info('\tfile "%s" extracted SUCCESSFULLY!', filename)


def main():
  # Command line options.
  parser = OptionParser()
  parser.add_option(
      '-l', '--log_level', metavar='LOG_LEVEL', default='error',
      help='LOG_LEVEL: debug, info, warning or error [default: %default]')

  options, args = parser.parse_args()
  options.log_level = options.log_level.upper()
  if options.log_level not in ['DEBUG', 'INFO', 'WARNING', 'ERROR']:
    print 'Wrong log_level argument.'
    parser.print_help()
    sys.exit(1)
  # Convert the capitalized log level string into the actual argument used by
  # the logging module.
  options.log_level = getattr(logging, options.log_level)

  extractor = FormsExtractor(logging_level=options.log_level)
  extractor.Extract()


if __name__ == '__main__':
  main()
