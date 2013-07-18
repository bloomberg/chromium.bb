# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from collections import defaultdict, deque, namedtuple
from HTMLParser import HTMLParser, HTMLParseError
import posixpath
from urlparse import urlsplit

from file_system_util import CreateURLsFromPaths
import svn_constants

Page = namedtuple('Page', 'status, links, anchors, anchor_refs')

def _SplitAnchor(url):
  components = urlsplit(url)
  return components.path, components.fragment

def _Process(path, renderer):
  '''Render the page at |path| using a |renderer| and process the contents of
  that page. Returns a |Page| namedtuple with fields for the http status code
  of the page render, the href of all the links that occurred on the page, all
  of the anchors on the page (ids and names), and all links that contain an
  anchor component.

  If a non-html page is properly rendered, a |Page| with status code 200 and
  all other fields empty is returned.
  '''
  parser = _ContentParser()
  response = renderer(path)

  if response.status != 200:
    return Page(response.status, (), (), ())
  if not path.endswith('.html'):
    return Page(200, (), (), ())

  try:
    parser.feed(str(response.content))
  except HTMLParseError:
    return Page(200, (), (), ())

  links, anchors = parser.links, parser.anchors
  base, _ = path.rsplit('/', 1)
  edges = []
  anchor_refs = []

  # Convert relative links to absolute links and categorize links as edges
  # or anchor_refs.
  for link in links:
    # Files like experimental_history.html are refered to with the URL
    # experimental.history.html.
    head, last = link.rsplit('/', 1) if '/' in link else ('', link)
    last, anchor = _SplitAnchor(last)

    if last.endswith('.html') and last.count('.') > 1:
      last = last.replace('.', '_', last.count('.') - 1)
      link = posixpath.join(head, last)
      if anchor:
        link = '%s#%s' % (link, anchor)

    if link.startswith('#'):
      anchor_refs.append(link)
    else:
      if link.startswith('/'):
        link = link[1:]
      else:
        link = posixpath.normpath('%s/%s' % (base, link))

      if '#' in link:
        anchor_refs.append(link)
      else:
        edges.append(link)

  return Page(200, edges, anchors, anchor_refs)

def _CategorizeBrokenLinks(url, page, pages):
  '''Find all the broken links on a page and categorize them as either
  broken_links, which link to a page that 404s, or broken_anchors. |page| is
  the page to search at |url|, |pages| is a callable that takes a path and
  returns a Page. Returns two lists, the first of all the broken_links, the
  second of all the broken_anchors.
  '''
  broken_links = []
  broken_anchors = []

  # First test links without anchors.
  for link in page.links:
    if pages(link).status != 200:
      broken_links.append((url, link))

  # Then find broken links with an anchor component.
  for ref in page.anchor_refs:
    path, anchor = _SplitAnchor(ref)

    if path == '':
      if not anchor in page.anchors and anchor != 'top':
        broken_anchors.append((url, ref))
    else:
      target_page = pages(path)
      if target_page.status != 200:
        broken_links.append((url, ref))
      elif not anchor in target_page.anchors:
        broken_anchors.append((url, ref))

  return broken_links, broken_anchors

class _ContentParser(HTMLParser):
  '''Parse an html file pulling out all links and anchor_refs, where an
  anchor_ref is a link that contains an anchor.
  '''

  def __init__(self):
    HTMLParser.__init__(self)
    self.links = []
    self.anchors = set()

  def handle_starttag(self, tag, raw_attrs):
    attrs = dict(raw_attrs)

    if tag == 'a':
      # Handle special cases for href's that: start with a space, contain
      # just a '.' (period), contain python templating code, are an absolute
      # url, are a zip file, or execute javascript on the page.
      href = attrs.get('href', '').strip()
      if href and not href == '.' and not '{{' in href:
        if not urlsplit(href).scheme in ('http', 'https'):
          if not href.endswith('.zip') and not 'javascript:' in href:
            self.links.append(href)

    if attrs.get('id'):
      self.anchors.add(attrs['id'])
    if attrs.get('name'):
      self.anchors.add(attrs['name'])

class LinkErrorDetector(object):
  '''Finds link errors on the doc server. This includes broken links, those with
  a target page that 404s or contain an anchor that doesn't exist, or pages that
  have no links to them.
  '''

  def __init__(self, file_system, renderer, public_path, root_pages):
    '''Creates a new broken link detector. |renderer| is a callable that takes
    a path and returns a full html page. |public_path| is the path to public
    template files. All URLs in |root_pages| are used as the starting nodes for
    the orphaned page search.
    '''
    self._file_system = file_system
    self._renderer = renderer
    self._public_path = public_path
    self._pages = defaultdict(lambda: Page(404, (), (), ()))
    self._root_pages = frozenset(root_pages)
    self._always_detached = frozenset(('apps/404.html', 'extensions/404.html'))

    self._RenderAllPages()

  def _RenderAllPages(self):
    '''Traverses the public templates directory rendering each URL and
    processing the resultant html to pull out all links and anchors.
    '''
    top_level_directories = (
      (svn_constants.PUBLIC_TEMPLATE_PATH, ''),
      (svn_constants.STATIC_PATH, 'static/'),
      (svn_constants.EXAMPLES_PATH, 'extensions/examples/'),
    )

    for dirpath, urlprefix in top_level_directories:
      files = CreateURLsFromPaths(self._file_system, dirpath, urlprefix)
      for url, path in files:
        self._pages[url] = _Process(url, self._renderer)

        if self._pages[url].status != 200:
          print(url, ', a url derived from the path', dirpath +
              ', resulted in a', self._pages[url].status)

  def GetBrokenLinks(self):
    '''Finds all broken links. A broken link is a link that leads to a page that
    does not exist (404s when rendered) or that contains an anchor that does not
    properly resolve.

    Returns a pair of lists, the first all of the links that lead to a
    non-existant page, the second all of the links that contain a broken
    anchor. Each item in the lists is a tuple of the page a broken link
    occurred on and the href of the broken link.
    '''
    broken_links = []
    broken_anchors = []

    for url in self._pages.keys():
      page = self._pages[url]
      if page.status != 200:
        continue
      links, anchors = _CategorizeBrokenLinks(
          url, page, lambda x: self._pages[x])

      broken_links.extend(links)
      broken_anchors.extend(anchors)

    return broken_links, broken_anchors

  def GetOrphanedPages(self):
    '''Crawls the server find all pages that are connected to the pages at
    |seed_url|s. Return the links that are valid on the server but are not in
    part of the connected component containing the |root_pages|. These pages
    are orphans and cannot be reached simply by clicking through the server.
    '''
    pages_to_check = deque(self._root_pages)
    found = set(self._root_pages) | self._always_detached

    while pages_to_check:
      item = pages_to_check.popleft()
      for link in self._pages[item].links:
        if link not in found:
          found.add(link)
          pages_to_check.append(link)

    all_urls = set(
        [url for url, page in self._pages.iteritems() if page.status == 200])

    return [url for url in all_urls - found if url.endswith('.html')]
