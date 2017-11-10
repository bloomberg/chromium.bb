// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions;

import org.chromium.chrome.browser.ntp.cards.ItemViewType;
import org.chromium.chrome.browser.ntp.cards.NewTabPageViewHolder;
import org.chromium.chrome.browser.ntp.cards.NodeVisitor;
import org.chromium.chrome.browser.ntp.cards.OptionalLeaf;

/**
 * A logo for the search engine provider.
 */
public class LogoItem extends OptionalLeaf {
    @Override
    @ItemViewType
    protected int getItemViewType() {
        return ItemViewType.LOGO;
    }

    @Override
    protected void onBindViewHolder(NewTabPageViewHolder holder) {}

    @Override
    public void visitOptionalItem(NodeVisitor visitor) {
        visitor.visitLogo();
    }

    public void setVisible(boolean visible) {
        setVisibilityInternal(visible);
    }
}
