// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function containsFeed(doc) {
  // Find all the RSS link elements.
  var result = doc.evaluate(
      '//*[local-name()="rss" or local-name()="feed" or local-name()="RDF"]',
      doc, null, 0, null);

  if (!result)
    return false;  // This is probably overly defensive, but whatever.

  var node = result.iterateNext();

  if (!node)
    return false;  // No RSS tags were found.

  // The feed for arab dash jokes dot net, for example, contains
  // a feed that is a child of the body tag so we continue only if the
  // node contains no parent or if the parent is the body tag.
  if (node.parentElement && node.parentElement.tagName != "BODY")
    return false;

  return true;
}
