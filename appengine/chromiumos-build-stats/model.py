# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""All database model classes for this AppEngine instance."""

from google.appengine.ext import db

class Statistics(db.Model):
  """Each entry holds stats for one build command run."""

  # Properties common to all commands.
  end_datetime = db.DateTimeProperty(auto_now_add=True)
  end_date = db.DateProperty()
  end_time = db.TimeProperty()
  cmd_line = db.StringProperty()
  cmd_base = db.StringProperty()
  cmd_args = db.StringProperty()
  run_time = db.IntegerProperty()
  username = db.StringProperty()
  board = db.StringProperty()
  host = db.StringProperty()
  cpu_count = db.StringProperty()
  cpu_type = db.StringProperty()

  # Properties for build_packages only.
  package_count = db.IntegerProperty()
