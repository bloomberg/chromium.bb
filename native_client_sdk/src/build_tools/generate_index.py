# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

HTML_TOP = '''
<!--
  Copyright (c) 2012 The Chromium Authors. All rights reserved.
  Use of this source code is governed by a BSD-style license that can be
  found in the LICENSE file.
-->

<!DOCTYPE html>
<html>
<head>
<style type="text/css">
dt {
  font-weight: bold;
}
dd {
  margin-bottom: 12pt;
  width: 800px;
}
</style>
<link href="http://code.google.com/css/codesite.css" rel="stylesheet"
 type="text/css" />
<title>Native Client Examples</title>
</head>
<body>
<h1>Native Client Examples</h1>
<dd><p>This page lists all of the examples available in the most recent Native
 Client SDK bundle. Each example is designed to teach a few specific Native
 Client programming concepts.  You will need to setup the build environment
 including a path to 'make' which can be found in the 'tools' directory for
 Windows, and the variable NACL_SDK_ROOT which points to one of the pepper
 bundles found under the SDK install location.  Calling make from the examples
 directory will build all the projects, while calling make from an individual
 example directory will build only that example.
</p></dd>
'''

HTML_END = '''
</body>
</html>
'''

SECTIONS = {
  'API': """
<h3>Common APIs</h3>
<dd><p>The following set of examples illustrate various Pepper APIs including
audio, 2D, 3D, file I/O, input and urls.</p></dd>
 """,
  'Concepts': """
<h3>Common Concepts</h3>
<dd><p>The following set of examples illustrate various common concepts such as
showing load progress, using Shared Objects (dynamic libraries),
mulithreading...</p></dd>
""",
  'Tools': """
<h3>Using the Tools</h3>
<dd><p>The following "hello_world" examples, show the basic outline of a
several types of Native Client applications.   The simplest, "Hello World Stdio"
uses several provided libraries to simplify startup and provides a
simplified make showing a single build configuration.  The other examples in
this SDK however, are designed to build and run with multiple toolsets, build
configurations, etc...
making the build much more complex.  In all cases we are using <a
href="http://www.gnu.org/software/make/manual/make.html">GNU Make</a>.
See the link for further information.
</p></dd>
""",
}


class LandingPage(object):
  def __init__(self):
    self.section_list = ['Tools', 'API', 'Concepts']
    self.section_map = {}
    for section in self.section_list:
      self.section_map[section] = []

  def _ExampleDescription(self, index_path, title, details, focus):
    return '''
  <dt><a href="%s/index.html">%s</a></dt>
  <dd>%s
  <p>Teaching focus: %s</p>
  </dd>
''' % (index_path, title, details, focus)

  def _GenerateSection(self, section):
    out = SECTIONS[section]
    for desc in self.section_map[section]:
      index_path = desc['NAME']
      title = desc['TITLE']
      details = desc['DESC']
      focus = desc['FOCUS']
      out += self._ExampleDescription(index_path, title, details, focus)
    return out

  def GeneratePage(self):
    out = HTML_TOP
    for section in self.section_list:
      out += self._GenerateSection(section)
    out += HTML_END
    return out

  def AddDesc(self, desc):
    group = desc['GROUP']
    assert group in self.section_list
    self.section_map[group].append(desc)

