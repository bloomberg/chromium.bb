#!/usr/bin/env python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from document_parser import ParseDocument, RemoveTitle


_WHOLE_DOCUMENT = '''
Preamble before heading.

<h1 id='main' class='header'>Main header</h1>
Some intro to the content.

<h2 id='banana' class='header'>Bananas</h2>
Something about bananas.

<h2 id='orange'>Oranges</h2>
Something about oranges.

<h3 id='valencia'>Valencia Oranges</h3>
A description of valencia oranges.

<h3 id='seville'>Seville Oranges</h3>
A description of seville oranges.

<h2>Grapefruit</h3>
Grapefruit closed a h2 with a h3. This should be a warning.

<h1 id='not-main'>Not the main header</h1>
But it should still show up in the TOC as though it were an h2.

<h2>Not <h3>a banana</h2>
The embedded h3 should be ignored.

<h4>It's a h4</h4>
h4 are not considered part of the document structure.

<h3>Plantains</h3>
Now I'm just getting lazy.
'''


_WHOLE_DOCUMENT_WITHOUT_TITLE = '''
Preamble before heading.


Some intro to the content.

<h2 id='banana' class='header'>Bananas</h2>
Something about bananas.

<h2 id='orange'>Oranges</h2>
Something about oranges.

<h3 id='valencia'>Valencia Oranges</h3>
A description of valencia oranges.

<h3 id='seville'>Seville Oranges</h3>
A description of seville oranges.

<h2>Grapefruit</h3>
Grapefruit closed a h2 with a h3. This should be a warning.

<h1 id='not-main'>Not the main header</h1>
But it should still show up in the TOC as though it were an h2.

<h2>Not <h3>a banana</h2>
The embedded h3 should be ignored.

<h4>It's a h4</h4>
h4 are not considered part of the document structure.

<h3>Plantains</h3>
Now I'm just getting lazy.
'''


class DocumentParserUnittest(unittest.TestCase):

  def testEmptyDocument(self):
    self.assertEqual(('', 'No opening <h1> was found'), RemoveTitle(''))

    result = ParseDocument('')
    self.assertEqual(None, result.title)
    self.assertEqual(None, result.title_attributes)
    self.assertEqual([], result.document_structure)
    self.assertEqual([], result.warnings)

    result = ParseDocument('', expect_title=True)
    self.assertEqual('', result.title)
    self.assertEqual({}, result.title_attributes)
    self.assertEqual([], result.document_structure)
    self.assertEqual(['Expected a title'], result.warnings)

  def testRemoveTitle(self):
    no_closing_tag = '<h1>No closing tag'
    self.assertEqual((no_closing_tag, 'No closing </h1> was found'),
                     RemoveTitle(no_closing_tag))

    no_opening_tag = 'No opening tag</h1>'
    self.assertEqual((no_opening_tag, 'No opening <h1> was found'),
                     RemoveTitle(no_opening_tag))

    tags_wrong_order = '</h1>Tags in wrong order<h1>'
    self.assertEqual((tags_wrong_order, 'The </h1> appeared before the <h1>'),
                     RemoveTitle(tags_wrong_order))

    multiple_titles = '<h1>First header</h1> and <h1>Second header</h1>'
    self.assertEqual((' and <h1>Second header</h1>', None),
                     RemoveTitle(multiple_titles))

    upper_case = '<H1>Upper case header tag</H1> hi'
    self.assertEqual((' hi', None), RemoveTitle(upper_case))
    mixed_case = '<H1>Mixed case header tag</h1> hi'
    self.assertEqual((' hi', None), RemoveTitle(mixed_case))

  def testOnlyTitleDocument(self):
    document = '<h1 id="header">heading</h1>'
    self.assertEqual(('', None), RemoveTitle(document))

    result = ParseDocument(document)
    self.assertEqual(None, result.title)
    self.assertEqual(None, result.title_attributes)
    self.assertEqual([], result.document_structure)
    self.assertEqual(['Found unexpected title "heading"'], result.warnings)

    result = ParseDocument(document, expect_title=True)
    self.assertEqual('heading', result.title)
    self.assertEqual({'id': 'header'}, result.title_attributes)
    self.assertEqual([], result.document_structure)
    self.assertEqual([], result.warnings)

  def testWholeDocument(self):
    self.assertEqual((_WHOLE_DOCUMENT_WITHOUT_TITLE, None),
                     RemoveTitle(_WHOLE_DOCUMENT))
    result = ParseDocument(_WHOLE_DOCUMENT, expect_title=True)
    self.assertEqual('Main header', result.title)
    self.assertEqual({'id': 'main', 'class': 'header'}, result.title_attributes)
    self.assertEqual([
      'Found closing </h3> while processing a <h2> (line 19, column 15)',
      'Found multiple <h1> tags. Subsequent <h1> tags will be classified as '
          '<h2> for the purpose of the structure (line 22, column 1)',
      'Found <h3> in the middle of processing a <h2> (line 25, column 9)',
    ], result.warnings)

    # The non-trivial table of contents assertions...
    entries = result.document_structure

    self.assertEqual(5, len(entries), entries)
    entry0, entry1, entry2, entry3, entry4 = entries

    self.assertEqual('Bananas', entry0.name)
    self.assertEqual({'id': 'banana', 'class': 'header'}, entry0.attributes)
    self.assertEqual([], entry0.entries)

    self.assertEqual('Oranges', entry1.name)
    self.assertEqual({'id': 'orange'}, entry1.attributes)
    self.assertEqual(2, len(entry1.entries))
    entry1_0, entry1_1 = entry1.entries

    self.assertEqual('Valencia Oranges', entry1_0.name)
    self.assertEqual({'id': 'valencia'}, entry1_0.attributes)
    self.assertEqual([], entry1_0.entries)
    self.assertEqual('Seville Oranges', entry1_1.name)
    self.assertEqual({'id': 'seville'}, entry1_1.attributes)
    self.assertEqual([], entry1_1.entries)

    self.assertEqual('Grapefruit', entry2.name)
    self.assertEqual({}, entry2.attributes)
    self.assertEqual([], entry2.entries)

    self.assertEqual('Not the main header', entry3.name)
    self.assertEqual({'id': 'not-main'}, entry3.attributes)
    self.assertEqual([], entry3.entries)

    self.assertEqual('Not a banana', entry4.name)
    self.assertEqual({}, entry4.attributes)
    self.assertEqual(1, len(entry4.entries))
    entry4_1, = entry4.entries

    self.assertEqual('Plantains', entry4_1.name)
    self.assertEqual({}, entry4_1.attributes)
    self.assertEqual([], entry4_1.entries)


if __name__ == '__main__':
  unittest.main()
