// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function editAnnotation(base_id_str) {
  document.getElementById("annotation_" + base_id_str + "_noedit").className =
      "hidden";
  document.getElementById("annotation_" + base_id_str + "_edit").className = "";
  return false;
}

function populateLatest() {
  var latest_build_id = document.getElementById("latest_build_id_cached").value;
  document.getElementById("id_latest_build_id").value = latest_build_id;
  return false;
}
