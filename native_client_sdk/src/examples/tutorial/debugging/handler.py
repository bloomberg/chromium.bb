# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Wrap the standard run.py to handle additional POST messages needed for
debugging.

See <NACL_SDK_ROOT>/tools/run.py for more information.
"""

import os
import SimpleHTTPServer  # pylint: disable=W0611
import sys
import urlparse

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
NACL_SDK_ROOT = os.path.dirname(os.path.dirname(SCRIPT_DIR))
TOOLS_DIR = os.path.join(NACL_SDK_ROOT, 'tools')

sys.path.append(TOOLS_DIR)
import decode_dump
import getos


last_nexe = None
last_nmf = None


# "Safely" split a string at |sep| into a [key, value] pair.  If |sep| does not
# exist in |pair|, then the entire |pair| is the key and the value is set to an
# empty string.
def KeyValuePair(pair, sep='='):
  if sep in pair:
    return pair.split(sep)
  else:
    return [pair, '']


# $(NACL_SDK_ROOT)/tools/run.py looks for a file named handler.py in any
# directory serving a file, then uses the class name HTTPRequestHandlerDelegate
# to handle GET and POST requests for that directory.
class HTTPRequestHandlerDelegate(object):
  def send_head(self, handler):
    """Common code for GET and HEAD commands.

    This sends the response code and MIME headers.

    Return value is either a file object (which has to be copied
    to the outputfile by the caller unless the command was HEAD,
    and must be closed by the caller under all circumstances), or
    None, in which case the caller has nothing further to do.

    """
    path = handler.translate_path(handler.path)
    f = None
    if os.path.isdir(path):
      if not handler.path.endswith('/'):
        # redirect browser - doing basically what apache does
        handler.send_response(301)
        handler.send_header("Location", handler.path + "/")
        handler.end_headers()
        return None
      for index in "index.html", "index.htm":
        index = os.path.join(path, index)
        if os.path.exists(index):
          path = index
          break
      else:
        return handler.list_directory(path)
    ctype = handler.guess_type(path)
    try:
      # Always read in binary mode. Opening files in text mode may cause
      # newline translations, making the actual size of the content
      # transmitted *less* than the content-length!
      f = open(path, 'rb')
    except IOError:
      handler.send_error(404, "File not found")
      return None
    handler.send_response(200)
    handler.send_header("Content-type", ctype)
    fs = os.fstat(f.fileno())
    handler.send_header("Content-Length", str(fs[6]))
    handler.send_header("Last-Modified", handler.date_time_string(fs.st_mtime))
    handler.send_header('Cache-Control','no-cache, must-revalidate')
    handler.send_header('Expires','-1')
    handler.end_headers()
    return f

  def do_GET(self, handler):
    global last_nexe, last_nmf
    (_, _, path, query, _) = urlparse.urlsplit(handler.path)
    url_params = dict([KeyValuePair(key_value)
                      for key_value in query.split('&')])
    if 'quit' in url_params and '1' in url_params['quit']:
      handler.send_response(200, 'OK')
      handler.send_header('Content-type', 'text/html')
      handler.send_header('Content-length', '0')
      handler.end_headers()
      handler.server.shutdown()
      return

    if path.endswith('.nexe'):
      last_nexe = path
    if path.endswith('.nmf'):
      last_nmf = path

    handler.base_do_GET()

  def do_POST(self, handler):
    if 'Content-Length' in handler.headers:
      if not NACL_SDK_ROOT:
        handler.wfile('Could not find NACL_SDK_ROOT to decode trace.')
        return
      data = handler.rfile.read(int(handler.headers['Content-Length']))
      nexe = '.' + last_nexe
      nmf = '.' + last_nmf
      addr = os.path.join(NACL_SDK_ROOT, 'toolchain',
                          getos.GetPlatform() + '_x86_newlib',
                          'bin', 'x86_64-nacl-addr2line')
      decoder = decode_dump.CoreDecoder(nexe, nmf, addr, None, None)
      info = decoder.Decode(data)
      trace = decoder.StackTrace(info)
      decoder.PrintTrace(trace, sys.stdout)
      decoder.PrintTrace(trace, handler.wfile)
