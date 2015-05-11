#!/usr/bin/env python
# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import cgi
# Enable debugging
import cgitb
cgitb.enable()

response = """Content-type: text/html

<html>
  <head>
    <script>
      function doRedirectAssign() {
        location.href = '%(url)s';
      }
      function doRedirectReplace() {
        location.replace('%(url)s');
      }
    </script>
  </head>
  <body onLoad="setTimeout('doRedirect%(redirect_function)s()', %(timeout)d);">
    This page will take you to "%(url)s" .
    <br>
    Relax and enjoy the ride.
  </body>
</html>
"""

def main():
  form = cgi.FieldStorage()
  url = form['location'].value
  if 'timeout' in form:
    timeout = int(form['timeout'].value)
  else:
    timeout = 0

  redirect_function = 'Replace'
  if 'kind' in form and form['kind'].value.upper() == 'ASSIGN':
      redirect_function = 'Assign'

  print (response % {
      'url': url,
      'timeout': timeout,
      'redirect_function': redirect_function })

main()
