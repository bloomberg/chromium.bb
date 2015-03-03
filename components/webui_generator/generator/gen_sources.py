# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import os.path
import sys

from declaration import Declaration
import export_h
import util
import view_model
import web_ui_view

args = util.CreateArgumentParser().parse_args()
declaration_path = os.path.relpath(args.declaration, args.root)
destination = os.path.relpath(args.destination, args.root)
os.chdir(args.root)
try:
  declaration = Declaration(declaration_path)
except Exception as e:
  print >> sys.stderr, e.message
  sys.exit(1)
view_model.Gen(declaration, destination)
web_ui_view.Gen(declaration, destination)
export_h.Gen(declaration, destination)

