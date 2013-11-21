# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Code shared by the various language-specific code generators."""

import mojom
import re

def GetStructFromMethod(interface, method):
  """Converts a method's parameters into the fields of a struct."""
  params_class = "%s_%s_Params" % (interface.name, method.name)
  struct = mojom.Struct(params_class)
  for param in method.parameters:
    struct.AddField(param.name, param.kind, param.ordinal)
  return struct

def IsPointerKind(kind):
  return isinstance(kind, (mojom.Struct, mojom.Array)) or kind.spec == 's'

def CamelToUnderscores(camel):
  s = re.sub('(.)([A-Z][a-z]+)', r'\1_\2', camel)
  return re.sub('([a-z0-9])([A-Z])', r'\1_\2', s).lower()

def StudlyCapsToCamel(studly):
  return studly[0].lower() + studly[1:]

class Generator(object):
  # Pass |output_dir| to emit files to disk. Omit |output_dir| to echo all files
  # to stdout.
  def __init__(self, module, header_dir, output_dir=None):
    self.module = module
    self.header_dir = header_dir
    self.output_dir = output_dir
