// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.photo_picker;

import android.os.Bundle;

import IDecoderServiceCallback;

/**
 * This interface is called by the Photo Picker to start image decoding jobs in
 * a separate process.
 */
interface IDecoderService {
  /**
   * Decode an image.
   * @param payload The data containing the details for the decoding request.
   */
  oneway void decodeImage(in Bundle payload, IDecoderServiceCallback listener);
}
