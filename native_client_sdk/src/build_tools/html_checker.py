#!/usr/bin/python
# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''This file provides a simple HTML checker, based on the standard HTMLParser
library.'''

import HTMLParser
import os
import sys


class HTMLChecker(HTMLParser.HTMLParser):
  '''A simple html parser that can find attribute tags and validate syntax'''

  def __init__(self):
    self.tag_list = []
    self.links = set()
    HTMLParser.HTMLParser.__init__(self)

  def handle_starttag(self, tag, attrs):
    attributes = dict(attrs)
    # Skip tags if they're within a <script> tag so that we don't get confused.
    if not self.tag_list or self.tag_list[-1] != 'script':
      self.tag_list.append(tag)
      if tag == 'a' and attributes.get('href'):
        self.links.add(attributes['href'])

  def handle_endtag(self, tag):
    try:
      matching_tag = self.tag_list.pop()
    except IndexError:
      raise Exception('Unmatched tag %s at %s' % (tag, self.getpos()))
    if matching_tag != tag:
      if matching_tag == 'script':
        self.tag_list.append(matching_tag)
      else:
        raise Exception('Wrong tag: Expected %s but got %s at %s' %
                        (matching_tag, tag, self.getpos()))

  def close(self):
    if self.tag_list:
      raise Exception('Reached end-of-file with unclosed tags: %s'
                      % self.tag_list)
    HTMLParser.HTMLParser.close(self)


def ValidateFile(filename):
  '''Run simple html syntax checks on given file to validate tags and links

  Args:
    filename: Name of file to validate

  Returns:
    tuple containing:
      set of urls from this file
      set of absolute paths from this file'''
  (directory, basename) = os.path.split(os.path.abspath(filename))
  parser = HTMLChecker()
  with open(filename, 'r') as file:
    parser.feed(file.read())
  parser.close()
  files = set()
  urls = set()
  for link in parser.links:
    if link.startswith('http://') or link.startswith('https://'):
      urls.add(link)
    else:
      files.add(os.path.abspath(os.path.join(directory, link)))
  return urls, files


def ValidateAllLinks(filenames):
  '''Validate all the links in filename and all linked files on this domain'''
  validated_files = set()
  validated_urls = set()
  need_to_validate = set([os.path.abspath(file) for file in filenames])
  while need_to_validate:
    file = need_to_validate.pop()
    print 'Evaluating %s' % file
    urls, files = ValidateFile(file)
    validated_files.add(file)
    need_to_validate |= files - validated_files


def main(argv):
  '''Run ValidateFile on each argument

  Args:
    argv: Command-line arguments'''
  ValidateAllLinks(argv)
  return 0

if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))