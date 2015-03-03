# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Provides various information needed for GYP and GN to generate and build
# files.

import os
import sys

from declaration import Declaration
import export_h
import util
import view_model
import web_ui_view

def GetImportDependencies(declaration):
  return set(child.build_target for child in declaration.children.itervalues())

parser = util.CreateArgumentParser()
parser.add_argument('--output',
                    choices=['view_cc', 'view_h', 'model_cc', 'model_h',
                             'export_h', 'dirname', 'target_name', 'imports',
                             'impl_macro', 'import_dependencies',
                             'list_outputs'],
                    required=True,
                    help='Type of output')
parser.add_argument('--gn', action='store_true',
                    help='Is called by GN')
args = parser.parse_args()
declaration_path = os.path.relpath(args.declaration, args.root)
os.chdir(args.root)
try:
  declaration = Declaration(declaration_path)
except Exception as e:
  print >> sys.stderr, e.message
  sys.exit(1)

if args.output == 'view_cc':
  print declaration.webui_view_cc_name
elif args.output == 'view_h':
  print declaration.webui_view_h_name
elif args.output == 'model_cc':
  print declaration.view_model_cc_name
elif args.output == 'model_h':
  print declaration.view_model_h_name
elif args.output == 'export_h':
  print declaration.export_h_name
elif args.output == 'dirname':
  print os.path.dirname(declaration_path)
elif args.output == 'target_name':
  print declaration.build_target
elif args.output == 'imports':
  for i in declaration.imports:
    print '//' + i
elif args.output == 'import_dependencies':
  for d in GetImportDependencies(declaration):
    print (':' if args.gn else '') + d
elif args.output == 'list_outputs':
  outputs = web_ui_view.ListOutputs(declaration, args.destination) + \
            view_model.ListOutputs(declaration, args.destination) + \
            export_h.ListOutputs(declaration, args.destination)
  for output in outputs:
    print output
elif args.output == 'impl_macro':
  print declaration.component_impl_macro
else:
  assert False

