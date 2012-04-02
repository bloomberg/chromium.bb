# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from google.appengine.ext import webapp
from google.appengine.ext.webapp import util

import stats

# Application configuration.
URLS = [
  ('/', webapp.RedirectHandler.new_factory('/stats', permanent=False)),
  ('/stats', stats.MainPage),
  ('/upload_command_stats', stats.PostPage),
]
APPLICATION = webapp.WSGIApplication(URLS, debug=True)


def main():
  """Manages and displays chromiumos build statistics."""
  util.run_wsgi_app(APPLICATION)


if __name__ == "__main__":
  main()
