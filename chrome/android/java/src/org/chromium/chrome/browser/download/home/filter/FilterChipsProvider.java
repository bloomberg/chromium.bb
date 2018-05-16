// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.filter;

import org.chromium.base.ObserverList;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.download.home.filter.Filters.FilterType;
import org.chromium.chrome.browser.download.home.filter.chips.Chip;
import org.chromium.chrome.browser.download.home.filter.chips.ChipsProvider;

import java.util.ArrayList;
import java.util.List;

/**
 * A {@link ChipsProvider} implementation that wraps a subset of {@link Filters} to be used as a
 * chip selector for filtering downloads.
 * TODO(dtrainor): Hook in as an observer to the download list data source.
 */
public class FilterChipsProvider implements ChipsProvider {
    private static final int INVALID_INDEX = -1;

    private final ObserverList<Observer> mObservers = new ObserverList<ChipsProvider.Observer>();
    private final List<Chip> mSortedChips = new ArrayList<>();

    /** Builds a new FilterChipsBackend. */
    public FilterChipsProvider() {
        Chip noneChip = new Chip(Filters.NONE, R.string.download_manager_ui_all_downloads,
                R.string.download_manager_ui_all_downloads, R.drawable.ic_play_arrow_white_24dp,
                () -> onChipSelected(Filters.NONE));
        Chip videosChip = new Chip(Filters.VIDEOS, R.string.download_manager_ui_video,
                R.string.download_manager_ui_video, R.drawable.ic_play_arrow_white_24dp,
                () -> onChipSelected(Filters.VIDEOS));
        Chip musicChip = new Chip(Filters.MUSIC, R.string.download_manager_ui_audio,
                R.string.download_manager_ui_audio, R.drawable.ic_play_arrow_white_24dp,
                () -> onChipSelected(Filters.MUSIC));
        Chip imagesChip = new Chip(Filters.IMAGES, R.string.download_manager_ui_images,
                R.string.download_manager_ui_images, R.drawable.ic_play_arrow_white_24dp,
                () -> onChipSelected(Filters.IMAGES));
        Chip sitesChip = new Chip(Filters.SITES, R.string.download_manager_ui_pages,
                R.string.download_manager_ui_pages, R.drawable.ic_play_arrow_white_24dp,
                () -> onChipSelected(Filters.SITES));
        Chip otherChip = new Chip(Filters.OTHER, R.string.download_manager_ui_other,
                R.string.download_manager_ui_other, R.drawable.ic_play_arrow_white_24dp,
                () -> onChipSelected(Filters.OTHER));

        mSortedChips.add(noneChip);
        mSortedChips.add(videosChip);
        mSortedChips.add(musicChip);
        mSortedChips.add(imagesChip);
        mSortedChips.add(sitesChip);
        mSortedChips.add(otherChip);
    }

    /**
     * Sets whether or not a filter is enabled.
     * @param type    The type of filter to enable.
     * @param enabled Whether or not that filter is enabled.
     */
    public void setFilterEnabled(@FilterType int type, boolean enabled) {
        int chipIndex = getChipIndex(type);
        if (chipIndex == INVALID_INDEX) return;
        Chip chip = mSortedChips.get(chipIndex);

        if (enabled == chip.enabled) return;
        chip.enabled = enabled;
        for (Observer observer : mObservers) observer.onChipChanged(chipIndex, chip);
    }

    /**
     * Sets the filter that is currently selected.
     * @param type The type of filter to select.
     */
    public void setFilterSelected(@FilterType int type) {
        for (int i = 0; i < mSortedChips.size(); i++) {
            Chip chip = mSortedChips.get(i);
            boolean willSelect = chip.id == type;

            // Early out if we're already selecting the appropriate Chip type.
            if (chip.selected && willSelect) return;
            if (chip.selected == willSelect) continue;
            chip.selected = willSelect;

            for (Observer observer : mObservers) observer.onChipChanged(i, chip);
        }
    }

    // ChipsProvider implementation.
    @Override
    public void addObserver(Observer observer) {
        mObservers.addObserver(observer);
    }

    @Override
    public void removeObserver(Observer observer) {
        mObservers.removeObserver(observer);
    }

    @Override
    public List<Chip> getChips() {
        return mSortedChips;
    }

    private void onChipSelected(int id) {
        setFilterSelected(id);
        // TODO(dtrainor): Notify external sources.
    }

    private int getChipIndex(@FilterType int type) {
        for (int i = 0; i < mSortedChips.size(); i++) {
            if (mSortedChips.get(i).id == type) return i;
        }

        return INVALID_INDEX;
    }
}