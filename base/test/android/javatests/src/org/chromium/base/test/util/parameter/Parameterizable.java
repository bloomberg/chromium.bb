// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.util.parameter;

import java.util.Map;

/**
 * An interface to implement on test cases to run {@link ParameterizedTest}s.
 */
public interface Parameterizable {
    Map<String, BaseParameter> getAvailableParameters();
    void setParameterReader(Parameter.Reader parameterReader);
}
