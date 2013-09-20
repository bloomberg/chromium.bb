# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


class Verifier:
  """Verifies that the current machine states match the expectation."""

  def Verify(self, verifier_input, variable_expander):
    """Verifies that the current machine states match |verifier_input|.

    This is an abstract method for subclasses to override.

    Args:
      verifier_input: An input to the verifier. Each subclass can specify a
          different input to the verifier.
      variable_expander: A VariableExpander object.
    """
    raise NotImplementedError()
