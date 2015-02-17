// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer('viewer-zoom-button', Polymer.mixin({
  // TODO(alexnadrec): the 'active' attribute is interfering with the
  // transition. Figure out why this is happening.
  activeChanged: function() {
    if (this.active)
      this.active = false;
  }
}, transitionCenterMixin));
