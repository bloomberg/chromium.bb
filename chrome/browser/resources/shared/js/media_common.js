// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function pathIsVideoFile(path) {
  return /\.(mp4|ogg|mpg|avi|mov)$/i.test(path);
}

function pathIsAudioFile(path) {
  return /\.(mp3|m4a|wav)$/i.test(path);
}

function pathIsImageFile(path) {
  return /\.(jpg|png|gif)$/i.test(path);
}

function pathIsHtmlFile(path) {
  return /\.(htm|html|txt)$/i.test(path);
}
