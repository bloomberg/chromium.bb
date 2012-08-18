#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Start an HTTP server which serves Chrome Endure graphs.

Usage:
    python endure_server.py [options]

To view Chrome Endure graphs from a browser,
run this script to start a local HTTP server that serves the directory
where graph code and test results are located. A port will be automatically
picked. You can then view the graphs via http://localhost:<GIVEN_PORT>.

Examples:
    >python endure_server.py
      Start a server which serves the default location
      <CURRENT_WORKING_DIR>/chrome_graph.

    >python endure_server.py --graph-dir=/home/user/Document/graph_dir
      Start a server which serves /home/user/Document/graph_dir which
      is where your graph code and test results are.
"""

import BaseHTTPServer
import logging
import optparse
import os
import SimpleHTTPServer
import sys


class HelpFormatter(optparse.IndentedHelpFormatter):
  """Format the help message of this script."""

  def format_description(self, description):
    """Override to keep original format of the description."""
    return description + '\n' if description else ''


def _ParseArgs(argv):
  parser = optparse.OptionParser(
      usage='%prog [options]',
      formatter=HelpFormatter(),
      description=__doc__)
  parser.add_option(
      '-g', '--graph-dir', type='string',
      default=os.path.join(os.getcwd(), 'chrome_graph'),
      help='The directory that contains graph code ' \
           'and data files of test results. Default value is ' \
           '<CURRENT_WORKING_DIR>/chrome_graph')
  return parser.parse_args(argv)


def Run(argv):
  """Start an HTTP server which serves Chrome Endure graphs."""
  logging.basicConfig(format='[%(levelname)s] %(message)s', level=logging.DEBUG)
  options, _ = _ParseArgs(argv)
  graph_dir = os.path.abspath(options.graph_dir)
  cur_dir = os.getcwd()
  os.chdir(graph_dir)
  httpd = BaseHTTPServer.HTTPServer(
      ('', 0), SimpleHTTPServer.SimpleHTTPRequestHandler)
  try:
    logging.info('Serving %s at port %d', graph_dir, httpd.server_port)
    logging.info('View graphs at http://localhost:%d', httpd.server_port)
    logging.info('Press Ctrl-C to stop the server.')
    httpd.serve_forever()
  except KeyboardInterrupt:
    logging.info('Shutting down ...')
    httpd.shutdown()
  finally:
    os.chdir(cur_dir)
  return 0


if '__main__' == __name__:
  sys.exit(Run(sys.argv[1:]))
