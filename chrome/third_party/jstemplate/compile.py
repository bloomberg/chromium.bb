#!/usr/bin/env python
# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# A python script that combines the javascript files needed by jstemplate into
# a single file.  Writes to standard output; you are responsible for putting
# the file into the tree where it belongs.

import httplib
import sys
import urllib

srcs ="util.js jsevalcontext.js jstemplate.js exports.js".split()

# Wrap the output in an anonymous function to prevent poluting the global
# namespace.
output_wrapper = "(function(){%s})()"

# Define the parameters for the POST request and encode them in a URL-safe
# format. See http://code.google.com/closure/compiler/docs/api-ref.html for API
# reference.
params = urllib.urlencode(
  map(lambda src: ('js_code', file(src).read()), srcs) +
  [
    ('compilation_level', 'ADVANCED_OPTIMIZATIONS'),
    ('output_format', 'text'),
    ('output_info', 'compiled_code'),
  ])

# Always use the following value for the Content-type header.
headers = {'Content-type': 'application/x-www-form-urlencoded'}
conn = httplib.HTTPConnection('closure-compiler.appspot.com')
conn.request('POST', '/compile', params, headers)
response = conn.getresponse()
sys.stdout.write(output_wrapper % response.read())
conn.close()
