// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function MetadataParser(parent) {
  this.parent_ = parent;
  this.verbose = parent.verbose;
}

MetadataParser.prototype.error = function(var_args) {
  this.parent_.error.apply(this.parent_, arguments);
};

MetadataParser.prototype.log = function(var_args) {
  this.parent_.log.apply(this.parent_, arguments);
};

MetadataParser.prototype.vlog = function(var_args) {
  if (this.verbose)
    this.parent_.log.apply(this.parent_, arguments);
};
