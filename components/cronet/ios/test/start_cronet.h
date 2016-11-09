// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

namespace cronet {

// Starts Cronet if it hasn't already been started. Will always update Cronet
// to point test.example.com" to "localhost:|port|".
void StartCronetIfNecessary(int port);

}  // namespace cronet
