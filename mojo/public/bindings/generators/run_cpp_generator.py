#!/usr/bin/env python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import ast
import mojom
import mojom_cpp_generator
import mojom_data
import mojom_pack
import sys

def ReadDict(file):
  with open(file, 'r') as f:
    s = f.read()
    dict = ast.literal_eval(s)
    return dict

dict = ReadDict(sys.argv[1])
module = mojom_data.ModuleFromData(dict)
dir = None
if len(sys.argv) > 2:
  dir = sys.argv[2]
cpp = mojom_cpp_generator.CPPGenerator(
    module, "mojo/public/bindings/generators/gen", dir)
cpp.GenerateFiles()
