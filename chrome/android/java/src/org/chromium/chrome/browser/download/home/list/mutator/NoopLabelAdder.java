// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.list.mutator;

import org.chromium.chrome.browser.download.home.list.ListItem;

import java.util.List;

/**
 * Implementation of {@link LabelAdder} that doesn't add any labels.
 */
public class NoopLabelAdder implements DateOrderedListMutator.LabelAdder {
    public NoopLabelAdder() {}

    @Override
    public List<ListItem> addLabels(List<ListItem> sortedList) {
        return sortedList;
    }
}
