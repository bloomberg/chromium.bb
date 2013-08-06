# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import registry_verifier

def Verify(property):
  """Verifies that the current machine states match the property object."""
  for verifier_name, value in property.iteritems():
    if verifier_name == 'RegistryEntries':
      registry_verifier.VerifyRegistryEntries(value)
    else:
      # TODO(sukolsak): Implement other verifiers
      # TODO(sukolsak): Use unittest framework instead of exceptions.
      raise Exception('Unknown verifier')
