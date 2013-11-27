# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from document_parser import ParseDocument


class DocumentRenderer(object):
  '''Performs document-level rendering such as the title and table of contents:
  pulling that data out of the document, then replacing the $(title) and
  $(table_of_contents) tokens with them.

  This can be thought of as a parallel to TemplateRenderer; while
  TemplateRenderer is responsible for interpreting templates and rendering files
  within the template engine, DocumentRenderer is responsible for interpreting
  higher-level document concepts like the title and TOC, then performing string
  replacement for them. The syntax for this replacement is $(...) where ... is
  the concept. Currently title and table_of_contents are supported.
  '''

  def __init__(self, table_of_contents_renderer):
    self._table_of_contents_renderer = table_of_contents_renderer

  def Render(self, document, render_title=False):
    parsed_document = ParseDocument(document, expect_title=render_title)
    toc_text, toc_warnings = self._table_of_contents_renderer.Render(
        parsed_document.sections)

    # Only 1 title and 1 table of contents substitution allowed; in the common
    # case, save necessarily running over the entire file.
    if parsed_document.title:
      document = document.replace('$(title)', parsed_document.title, 1)
    return (document.replace('$(table_of_contents)', toc_text, 1),
            parsed_document.warnings + toc_warnings)
