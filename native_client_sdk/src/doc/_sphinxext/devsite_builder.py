# Copyright (c) 2013 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

#
# This is a Sphinx extension.
#

import codecs
import os
import string
from docutils import nodes
from docutils.parsers.rst import Directive, directives
from sphinx.util.osutil import ensuredir
from sphinx.builders.html import StandaloneHTMLBuilder
from sphinx.writers.html import HTMLWriter
from sphinx.writers.html import SmartyPantsHTMLTranslator as HTMLTranslator


# TODO(eliben): it may be interesting to use an actual Sphinx template here at
# some point.
PAGE_TEMPLATE = string.Template(r'''
${devsite_prefix}
<html devsite>
  <head>
    <title>${doc_title}</title>
    <meta name="project_path" value="/native-client/_project.yaml" />
    <meta name="book_path" value="/native-client/_book.yaml" />
    <link href="/native-client/css/local_extensions.css" rel="stylesheet" type="text/css"/>
    ${nonprod_css}
    <style type="text/css">
      .nested-def {list-style-type: none; margin-bottom: 0.3em;}
      .maxheight {height: 200px;}
    </style>
  </head>
  <body>
    ${devsite_butterbar}

${doc_body}

  </body>
</html>
'''.lstrip())

DEVSITE_PREFIX = r'''
{% setvar pepperversion %}pepper28{% endsetvar %}
{% include "native-client/_local_variables.html" %}
'''.lstrip()

DEVSITE_BUTTERBAR = '{{butterbar}}'

# We want the non-production-mode HTML to resemble the real one, so this points
# to a copied version of the devsite CSS that we'll keep locally. It's for
# testing purposes only.
NONPROD_CSS = '<link href="/_static/css/local_extensions.css"'\
              'rel="stylesheet" type="text/css"/>'


class DevsiteHTMLTranslator(HTMLTranslator):
  """ Custom HTML translator for devsite output.

      Hooked into the HTML builder by setting the html_translator_class
      option in conf.py
  """
  def __init__(self, builder, *args, **kwds):
    # HTMLTranslator is an old-style Python class, so 'super' doesn't work: use
    # direct parent invocation.
    HTMLTranslator.__init__(self, builder, *args, **kwds)

    self.within_ignored_h1 = False
    self.within_toc = False

  def visit_bullet_list(self, node):
    if self.within_toc:
      # Within a TOC, devsite wants <ol>
      self.body.append(self.starttag(node, 'ol'))
    else:
      # Use our own class attribute for <ul>. Don't care about compacted lists.
      self.body.append(self.starttag(node, 'ul', **{'class': 'small-gap'}))

  def depart_bullet_list(self, node):
    if self.within_toc:
      self.body.append('</ol>\n')
    else:
      # Override to not pop anything from context
      self.body.append('</ul>\n')

  def visit_literal_block(self, node):
    # We don't use Sphinx's buildin pygments integration for code highlighting,
    # because the devsite requires special <pre> tags for that and handles the
    # highlighting on its own.
    attrs = {'class': 'prettyprint'} if node.get('prettyprint', 1) else {}
    self.body.append(self.starttag(node, 'pre', **attrs))

  def depart_literal_block(self, node):
    self.body.append('\n</pre>\n')

  def visit_paragraph(self, node):
    # Don't generate <p>s within the table of contents
    if not self.within_toc:
      HTMLTranslator.visit_paragraph(self, node)

  def depart_paragraph(self, node):
    if not self.within_toc:
      HTMLTranslator.depart_paragraph(self, node)

  def visit_section(self, node):
    # devsite needs <section> instead of <div class='section'>
    self.section_level += 1
    self.body.append(self.starttag(node, 'section'))

  def depart_section(self, node):
    self.section_level -= 1
    self.body.append('</section>')

  def visit_image(self, node):
    # Paths to images in .rst sources should be absolute. This visitor does the
    # required transformation for the path to be correct in the final HTML.
    if self.builder.devsite_production_mode:
      node['uri'] = '/native-client/' + node['uri']
    HTMLTranslator.visit_image(self, node)

  def visit_title(self, node):
    # Why this?
    #
    # Sphinx insists on inserting a <h1>Page Title</h1> into the page, but this
    # is not how the devsite wants it. The devsite inserts the page title on
    # its own, the the extra h1 is duplication.
    #
    # Killing the doctree title node in a custom transform doesn't work, because
    # Sphinx explicitly looks for it when writing a document. So instead we rig
    # the HTML produced.
    #
    # When a title node is visited, and this is the h1-to-be, we ignore it and
    # also set a flag that tells visit_Text not to print the actual text of the
    # header.

    # The h1 node is in the section whose parent is the document itself. Other
    # sections have this top-section as their parent.
    if (node.parent and node.parent.parent and
        isinstance(node.parent.parent, nodes.document)):
      # Internal flag. Also, nothing is pushed to the context. Our depart_title
      # doesn't pop anything when this flag is set.
      self.within_ignored_h1 = True
    else:
      HTMLTranslator.visit_title(self, node)

  def depart_title(self, node):
    if not self.within_ignored_h1:
      HTMLTranslator.depart_title(self, node)
    self.within_ignored_h1 = False

  def visit_Text(self, node):
    if not self.within_ignored_h1:
      HTMLTranslator.visit_Text(self, node)

  def visit_topic(self, node):
    if 'contents' in node['classes']:
      # Detect a TOC: this requires special treatment for devsite.
      self.within_toc = True
      # Emit <nav> manually and not through starttage because we don't want
      # additional class components to be added
      self.body.append('\n<nav class="inline-toc">')

  def depart_topic(self, node):
    if self.within_toc:
      self.within_toc = False
      self.body.append('</nav>\n')

  def write_colspecs(self):
    # Override this method from docutils to do nothing. We don't need those
    # pesky <col width=NN /> tags in our markup.
    pass

  def visit_admonition(self, node, name=''):
    self.body.append(self.starttag(node, 'aside', CLASS=node.get('class', '')))

  def depart_admonition(self, node=''):
    self.body.append('\n</aside>\n')

  def unknown_visit(self, node):
    raise NotImplementedError('Unknown node: ' + node.__class__.__name__)


class DevsiteBuilder(StandaloneHTMLBuilder):
  """ Builder for the NaCl devsite HTML output.

      Loosely based on the code of Sphinx's standard SerializingHTMLBuilder.
  """
  name = 'devsite'
  out_suffix = '.html'
  link_suffix = '.html'

  # Disable the addition of "pi"-permalinks to each section header
  add_permalinks = False

  def init(self):
    self.devsite_production_mode = int(self.config.devsite_production_mode) == 1
    print "----> Devsite builder with production mode = %d" % (
        self.devsite_production_mode,)
    self.config_hash = ''
    self.tags_hash = ''
    self.theme = None       # no theme necessary
    self.templates = None   # no template bridge necessary
    self.init_translator_class()
    self.init_highlighter()

  def get_target_uri(self, docname, typ=None):
    if self.devsite_production_mode:
      # TODO(eliben): testrst here will have to be replaced with
      # {{pepperversion}}
      return '/native-client/testrst/%s' % docname
    else:
      return docname + self.link_suffix

  def handle_page(self, pagename, ctx, templatename='page.html',
                  outfilename=None, event_arg=None):
    ctx['current_page_name'] = pagename

    if not outfilename:
      outfilename = os.path.join(self.outdir,
                                 pagename + self.out_suffix)

    # Emit an event to Sphinx
    self.app.emit('html-page-context', pagename, templatename,
                  ctx, event_arg)

    ensuredir(os.path.dirname(outfilename))
    self._dump_context(ctx, outfilename)

  def _dump_context(self, context, filename):
    """ Do the actual dumping of the page to the file. context is a dict. Some
        important fields:
          body - document contents
          title
          current_page_name
        Some special pages (genindex, etc.) may not have some of the fields, so
        fetch them conservatively.
    """
    if not 'body' in context:
      return
    # codecs.open is the fast Python 2.x way of emulating the encoding= argument
    # in Python 3's builtin open.
    with codecs.open(filename, 'w', encoding='utf-8') as f:
      f.write(PAGE_TEMPLATE.substitute(
        doc_title=context.get('title', ''),
        doc_body=context.get('body'),
        nonprod_css=self._conditional_nonprod(NONPROD_CSS),
        devsite_prefix=self._conditional_devsite(DEVSITE_PREFIX),
        devsite_butterbar=self._conditional_devsite(DEVSITE_BUTTERBAR)))

  def _conditional_devsite(self, s):
    return s if self.devsite_production_mode else ''

  def _conditional_nonprod(self, s):
    return s if not self.devsite_production_mode else ''


class NaclCodeDirective(Directive):
  """ Custom "naclcode" directive for code snippets. To keep it under our
      control.
  """
  has_content = True
  required_arguments = 0
  optional_arguments = 1
  option_spec = {
      'prettyprint': int,
  }

  def run(self):
    code = u'\n'.join(self.content)
    literal = nodes.literal_block(code, code)
    literal['prettyprint'] = self.options.get('prettyprint', 1)
    return [literal]


def setup(app):
  """ Extension registration hook.
  """
  app.add_directive('naclcode', NaclCodeDirective)
  app.add_builder(DevsiteBuilder)

  # "Production mode" for local testing vs. on-server documentation.
  app.add_config_value('devsite_production_mode', default='1', rebuild='html')


