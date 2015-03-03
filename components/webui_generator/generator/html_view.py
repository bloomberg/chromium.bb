# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import util
import os
import datetime

HTML_FILE_TEMPLATE = \
"""<!-- Copyright %(year)d The Chromium Authors. All rights reserved.
     Use of this source code is governed by a BSD-style license that can be
     found in the LICENSE file. -->

<!-- NOTE: this file is generated from "%(source)s". Do not modify directly. -->

<!doctype html>
<html><head>
<link rel="import" href="chrome://resources/polymer/polymer/polymer.html">
<link rel="import" href="/webui_generator/webui-view.html">
%(children_includes)s
</head><body>
<polymer-element name="%(element_name)s" extends="webui-view">
</polymer-element>
<script src="%(js_file_path)s"></script>
</body></html>
"""

JS_FILE_TEMPLATE = \
"""// Copyright %(year)d The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// NOTE: this file is generated from "%(source)s". Do not modify directly.


Polymer('%(element_name)s', (function() {
  'use strict';
  return {
  getType: function() {
    return '%(type)s';
  },

  initialize: function() {},
  contextChanged: function(changedKeys) {},

  initChildren_: function() {
%(init_children_body)s
  },

  initContext_: function() {
%(init_context_body)s
  }
  }
})());
"""

def GetCommonSubistitutions(declaration):
  subs = {}
  subs['year'] = datetime.date.today().year
  subs['element_name'] = declaration.type.replace('_', '-') + '-view'
  subs['source'] = declaration.path
  return subs

def GenChildrenIncludes(children):
  lines = []
  for declaration in set(children.itervalues()):
    lines.append('<link rel="import" href="/%s">' %
                     declaration.html_view_html_include_path)
  return '\n'.join(lines)

def GenHTMLFile(declaration):
  subs = GetCommonSubistitutions(declaration)
  subs['js_file_path'] = subs['element_name'] + '.js'
  subs['children_includes'] = GenChildrenIncludes(declaration.children)
  return HTML_FILE_TEMPLATE % subs

def GenInitChildrenBody(children):
  lines = []
  lines.append('    var child = null;');
  for id in children:
    lines.append('    child = this.shadowRoot.querySelector(\'[wugid="%s"]\');'
                     % id);
    lines.append('    if (!child)');
    lines.append('      console.error(this.path_ + \'$%s not found.\');' % id);
    lines.append('    else');
    lines.append('      child.setPath_(this.path_ + \'$%s\');' % id)
  return '\n'.join(lines)

def GenInitContextBody(fields):
  lines = []
  for field in fields:
    value = ''
    if field.type in ['integer', 'double']:
      value = str(field.default_value)
    elif field.type == 'boolean':
      value = 'true' if field.default_value else 'false'
    elif field.type == 'string':
      value = '\'%s\'' % field.default_value
    elif field.type == 'string_list':
      value = '[' + \
        ', '.join('\'%s\'' % s for s in field.default_value) + ']'
    lines.append('    this.context.set(\'%s\', %s);' % (field.id, value))
  lines.append('    this.context.getChangesAndReset();')
  return '\n'.join(lines)

def GenJSFile(declaration):
  subs = GetCommonSubistitutions(declaration)
  subs['type'] = declaration.type
  subs['init_children_body'] = GenInitChildrenBody(declaration.children)
  subs['init_context_body'] = GenInitContextBody(declaration.fields)
  return JS_FILE_TEMPLATE % subs
