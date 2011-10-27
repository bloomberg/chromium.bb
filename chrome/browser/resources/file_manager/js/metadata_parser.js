// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function MetadataParser(parent, type, urlFilter) {
  this.parent_ = parent;
  this.type = type;
  this.urlFilter = urlFilter;
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

MetadataParser.prototype.createDefaultMetadata = function() { return {} };

MetadataParser.prototype.acceptsMimeType = function(mimeType) { return false };


/* Base class for image metadata parsers */
function ImageParser(parent, type, urlFilter) {
  MetadataParser.apply(this, arguments);
  this.mimeType = 'image/' + this.type;
}

ImageParser.prototype = {__proto__: MetadataParser.prototype};

ImageParser.prototype.createDefaultMetadata = function() {
  return { mimeType: this.mimeType };
};

ImageParser.prototype.acceptsMimeType = function(mimeType) {
  return mimeType == this.mimeType;
}

