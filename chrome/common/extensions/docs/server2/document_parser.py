# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from HTMLParser import HTMLParser
import logging


class ParseResult(object):
  '''The result of |ParseDocument|:
  |title|              The title of the page, as pulled from the first <h1>.
  |title_attributes|   The attributes of the <h1> tag the title is derived from.
  |document_structure| A list of DocumentStringEntry objects.
  |warnings|           Any warnings while parsing the document.
  '''

  def __init__(self,
               title,
               title_attributes,
               document_structure,
               warnings):
    self.title = title
    self.title_attributes = title_attributes
    self.document_structure = document_structure
    self.warnings = warnings


class DocumentStructureEntry(object):
  '''An entry in the document structure.
  |attributes| The attributes of the header tag this entry is derived from.
  |name|       The name of this entry, as pulled from the header tag this entry
               is derived from.
  |entries|    A list of child DocumentStructureEntry items.
  '''

  def __init__(self, tag, attributes):
    self.attributes = attributes
    self.name = ''
    self.entries = []
    # Callers shouldn't care about the tag, but we need it for sanity checking,
    # so make it private. In particular we pretend that anything but the first
    # h1 is an h2, and it'd be odd to expose that.
    self._tag = tag

  def __repr__(self):
    return '<%s>%s</%s>' % (self._tag, self.name, self._tag)

  def __str__(self):
    return repr(self)


def ParseDocument(document, expect_title=False):
  '''Parses the title and a document structure form |document| and returns a
  ParseResult.
  '''
  parser = _DocumentParser(expect_title)
  parser.feed(document)
  parser.close()
  return parser.parse_result


def RemoveTitle(document):
  '''Removes the first <h1>..</h1> tag found in |document| and returns a
  (result, warning) tuple.

  If no title is found or |document| is malformed in some way, returns the
  original document and a warning message. Otherwise, returns the result of
  removing the title from |document| with a None warning message.
  '''

  def min_index(lhs, rhs):
    lhs_index, rhs_index = document.find(lhs), document.find(rhs)
    if lhs_index == -1: return rhs_index
    if rhs_index == -1: return lhs_index
    return min(lhs_index, rhs_index)

  title_start = min_index('<h1', '<H1')
  if title_start == -1:
    return document, 'No opening <h1> was found'
  title_end = min_index('/h1>', '/H1>')
  if title_end == -1:
    return document, 'No closing </h1> was found'
  if title_end < title_start:
    return document, 'The </h1> appeared before the <h1>'

  return (document[:title_start] + document[title_end + 4:], None)


_HEADER_TAGS = ['h2', 'h3']


class _DocumentParser(HTMLParser):
  '''HTMLParser for ParseDocument.
  '''

  def __init__(self, expect_title):
    HTMLParser.__init__(self)
    # Public.
    self.parse_result = None
    # Private.
    self._expect_title = expect_title
    self._title_entry = None
    self._document_structure = []
    self._warnings = []
    self._processing_entry = None

  def handle_starttag(self, tag, attrs):
    if tag != 'h1' and tag not in _HEADER_TAGS:
      return

    if self._processing_entry is not None:
      self._WarnWithPosition('Found <%s> in the middle of processing a <%s>' %
                             (tag, self._processing_entry._tag))
      return

    self._processing_entry = DocumentStructureEntry(tag, dict(attrs))

    if tag == 'h1' and self._title_entry is not None:
      self._WarnWithPosition('Found multiple <h1> tags. Subsequent <h1> tags '
                             'will be classified as <h2> for the purpose of '
                             'the structure')
      tag = 'h2'

    if tag == 'h1':
      self._title_entry = self._processing_entry
    else:
      belongs_to = self._document_structure
      for header in _HEADER_TAGS[:_HEADER_TAGS.index(tag)]:
        if len(belongs_to) == 0:
          self._WarnWithPosition('Found <%s> without any preceding <%s>' %
                                 (tag, header))
          break
        belongs_to = belongs_to[-1].entries
      belongs_to.append(self._processing_entry)

  def handle_endtag(self, tag):
    if tag != 'h1' and tag not in _HEADER_TAGS:
      return

    if self._processing_entry is None:
      self._WarnWithPosition('Found closing </%s> without an opening <%s>' %
                             (tag, tag))
      return

    if self._processing_entry._tag != tag:
      self._WarnWithPosition('Found closing </%s> while processing a <%s>' %
                             (tag, self._processing_entry._tag))
      # Note: no early return, it's more likely that the mismatched header was
      # a typo rather than a misplaced closing header tag.

    self._processing_entry = None

  def handle_data(self, data):
    if self._processing_entry is not None:
      # += is inefficient, but probably fine here because the chances of a
      # large number of nested tags within header tags is pretty low.
      self._processing_entry.name += data

  def close(self):
    HTMLParser.close(self)

    if self._processing_entry is not None:
      self._warnings.append('Finished parsing while still processing a <%s>' %
                            parser._processing_entry._tag)

    if self._expect_title:
      if not self._title_entry:
        self._warnings.append('Expected a title')
        title, title_attributes = '', {}
      else:
        title, title_attributes = (
            self._title_entry.name, self._title_entry.attributes)
    else:
      if self._title_entry:
        self._warnings.append('Found unexpected title "%s"' %
                              self._title_entry.name)
      title, title_attributes = None, None

    self.parse_result = ParseResult(
        title, title_attributes, self._document_structure, self._warnings)

  def _WarnWithPosition(self, message):
    line, col = self.getpos()
    self._warnings.append('%s (line %s, column %s)' % (message, line, col + 1))
