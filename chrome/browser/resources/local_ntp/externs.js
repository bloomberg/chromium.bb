// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Externs for objects sent from C++ to
 * chrome-search://local-ntp/local-ntp.html.
 * @externs
 */

/**
 * The type of the most visited data object. The definition is based on
 * chrome/common/search/instant_types.h:
 *     InstantMostVisitedItem
 *  @typedef {{dataGenerationTime: Date,
 *             isAddButton: boolean,
 *             tid: number,
 *             tileSource: number,
 *             tileTitleSource: number,
 *             title: ?,
 *             url: string}}
 */
let MostVisitedData;

/**
 * The type of the config data object. The definition is based on
 * chrome/browser/search/local_ntp_source.cc:
 *     LocalNtpSource::SearchConfigurationProvider::UpdateConfigData()
 * @typedef {{translatedStrings: Array<string>,
 *            isGooglePage: boolean,
 *            googleBaseUrl: string,
 *            isAccessibleBrowser: boolean,
 *            removeFakebox: boolean,
 *            alternateFakebox: boolean,
 *            fakeboxSearchIcon: boolean,
 *            hideShortcuts: boolean}}
 */
let configData;

/**
 * The type of the image collections object. The definition is based on
 * chrome/browser/search/local_ntp_source.cc:
 *     ConvertCollectionInfoToDict()
 * @typedef {{collectionId: string,
 *            collectionName: string,
 *            previewImageUrl: string}}
 */
let coll;

/**
 * The type of the individual image data object. The definition is based on
 * chrome/browser/search/local_ntp_source.cc:
 *     ConvertCollectionImageToDict()
 * @typedef {{thumbnailImageUrl: string,
 *            imageUrl: string,
 *            collectionId: string,
 *            attributions: Array<string>,
 *            attributionActionUrl: string}}
 */
let collImg;

/**
 * The type of the errors when fetching individual images object. The
 * definition is based on
 * chrome/browser/search/local_ntp_source.cc:
 *     GetErrorDict()
 * @typedef {{net_error: boolean,
 *            service_error: boolean,
 *            net_error_no: number}}
 */
let collImgErrors;

/**
 * The type of the errors when fetching collection object. The definition is
 * based on
 * chrome/browser/search/local_ntp_source.cc:
 *     GetErrorDict()
 * @typedef {{net_error: boolean,
 *            service_error: boolean,
 *            net_error_no: number}}
 */
let collErrors;

/**
 * The type of the Doodle data object. The definition is based on
 * chrome/browser/search/local_ntp_source.cc:
 *     LocalNtpSource::DesktopLogoObserver::OnLogoAvailable()
 * @typedef {{usable: boolean,
 *            image: string,
 *            metadata: string}}
 */
let ddl;

/**
 * The type of the OneGoogleBar data object. The definition is based on
 * chrome/browser/search/local_ntp_source.cc:
 *     ConvertOGBDataToDict()
 * @typedef {{barHtml: string,
 *            inHeadScript: string,
 *            inHeadStyle: string,
 *            afterBarScript: string,
 *            endOfBodyHtml: string,
 *            endOfBodyScript: string}}
 */
let og;

/**
 * The type of the middle-slot promo data object. The definition is based on
 * chrome/browser/search/local_ntp_source.cc:
 *     ConvertPromoDataToDict()
 * @typedef {{promoHtml: string,
 *            promoLogUrl: string}}
 */
let promo;

/**
 * The type of the search suggestions data object. The definition is based on
 * chrome/browser/search/local_ntp_source.cc:
 *     ConvertSearchSuggestDataToDict()
 * @typedef {{suggestionsHtml: string,
 *            suggestionsEndOfBodyScript: string}}
 */
let searchSuggestions;

/**
 * Types created by the OneGoogleBar scripts when injected into the page.
 * Defined in google3/javascript/externs/api/one_google/gbar.js
 */
window.gbar;
window.gbar.a;
window.gbar.a.pc;

/**
 * Used for running NTP javascript unit tests. Defined in
 * src/chrome/test/data/local_ntp/local_ntp_browsertest.html
 */
window.localNTPUnitTest;

/**************************** Embedded Search API ****************************/

/**
 * Embedded Search API methods defined in
 * chrome/renderer/searchbox/searchbox_extension.cc:
 *  NewTabPageBindings::GetObjectTemplateBuilder()
 */

window.chrome;
window.chrome.embeddedSearch;
window.chrome.embeddedSearch.newTabPage;

/**
 * @param {number} task_version
 * @param {number} task_id
 */
window.chrome.embeddedSearch.newTabPage.blacklistSearchSuggestion;

/**
 * @param {number} task_version
 * @param {number} task_id
 * @param {string} hash
 */
window.chrome.embeddedSearch.newTabPage.blacklistSearchSuggestionWithHash;

/**
 * @param {string} identity
 */
window.chrome.embeddedSearch.newTabPage.checkIsUserSignedIntoChromeAs;

/**
 * No params.
 */
window.chrome.embeddedSearch.newTabPage.checkIsUserSyncingHistory;

/**
 * @param {number} tid
 */
window.chrome.embeddedSearch.newTabPage.deleteMostVisitedItem;

/**
 * @param {string} url
 */
window.chrome.embeddedSearch.newTabPage.fixupAndValidateUrl;

/**
 * @param {number} tid
 */
window.chrome.embeddedSearch.newTabPage.getMostVisitedItemData;

/**
 * @return {boolean} isCustomLinks
 */
window.chrome.embeddedSearch.newTabPage.isCustomLinks;

/**
 * @return {boolean} isInputInProgress
 */
window.chrome.embeddedSearch.newTabPage.isInputInProgress;

/**
 * @param {number} event
 */
window.chrome.embeddedSearch.newTabPage.logEvent;

/**
 * @param {number} position
 * @param {number} tile_title_source
 * @param {number} tile_source
 * @param {number} tile_type
 * @param {number} data_generation_time
 */
window.chrome.embeddedSearch.newTabPage.logMostVisitedImpression;

/**
 * @param {number} position
 * @param {number} tile_title_source
 * @param {number} tile_source
 * @param {number} tile_type
 * @param {number} data_generation_time
 */
window.chrome.embeddedSearch.newTabPage.logMostVisitedNavigation;

/**
 * No params.
 */
window.chrome.embeddedSearch.newTabPage.mostVisited;

/**
 * @return {boolean} mostVisitedAvailable
 */
window.chrome.embeddedSearch.newTabPage.mostVisitedAvailable;

/**
 * No params.
 */
window.chrome.embeddedSearch.newTabPage.optOutOfSearchSuggestions;

/**
 * @param {number} rid
 * @param {number} new_pos
 */
window.chrome.embeddedSearch.newTabPage.reorderCustomLink;

/**
 * No params.
 */
window.chrome.embeddedSearch.newTabPage.resetCustomLinks;

/**
 * @param {number} task_version
 * @param {number} task_id
 * @param {string} hash
 */
window.chrome.embeddedSearch.newTabPage.searchSuggestionSelected;

/**
 * No params.
 */
window.chrome.embeddedSearch.newTabPage.selectLocalBackgroundImage;

/**
 * @param {string} background_url
 */
window.chrome.embeddedSearch.newTabPage.setBackgroundURL;

/**
 * @param {string} background_url
 * @param {string} attribution_line_1
 * @param {string} attribution_line_2
 * @param {string} attribution_action_url
 */
window.chrome.embeddedSearch.newTabPage.setBackgroundURLWithAttributions;

/**
 * @return {Object} theme_background_info
 */
window.chrome.embeddedSearch.newTabPage.themeBackgroundInfo;

/**
 * No params.
 */
window.chrome.embeddedSearch.newTabPage.undoAllMostVisitedDeletions;

/**
 * No params.
 */
window.chrome.embeddedSearch.newTabPage.undoCustomLinkAction;

/**
 * @param {number} rid_value
 */
window.chrome.embeddedSearch.newTabPage.undoMostVisitedDeletion;

/**
 * @param {number} rid
 * @param {string} url
 */
window.chrome.embeddedSearch.newTabPage.updateCustomLink;

/**
 * Embedded Search API methods defined in
 * chrome/renderer/searchbox/searchbox_extension.cc:
 *  SearchBoxBindings::GetObjectTemplateBuilder()
 */
window.chrome.embeddedSearch.searchBox;
window.chrome.embeddedSearch.searchBox.isKeyCaptureEnabled;
window.chrome.embeddedSearch.searchBox.paste;
window.chrome.embeddedSearch.searchBox.startCapturingKeyStrokes;
window.chrome.embeddedSearch.searchBox.stopCapturingKeyStrokes;


/**************************** Translated Strings *****************************/

/**
 * Translated strings defined in
 * chrome/browser/search/local_ntp_source.cc:
 *  GetTranslatedStrings()
 */

configData.translatedStrings.addLinkTitle;
configData.translatedStrings.addLinkTooltip;
configData.translatedStrings.attributionIntro;
configData.translatedStrings.audioError;
configData.translatedStrings.backLabel;
configData.translatedStrings.backgroundsUnavailable;
configData.translatedStrings.clickToViewDoodle;
configData.translatedStrings.connectionError;
configData.translatedStrings.connectionErrorNoPeriod;
configData.translatedStrings.copyLink;
configData.translatedStrings.customizeBackground;
configData.translatedStrings.customizeButtonLabel;
configData.translatedStrings.customizeThisPage;
configData.translatedStrings.dailyRefresh;
configData.translatedStrings.defaultWallpapers;
configData.translatedStrings.details;
configData.translatedStrings.editLinkTitle;
configData.translatedStrings.editLinkTooltip;
configData.translatedStrings.fakeboxMicrophoneTooltip;
configData.translatedStrings.invalidUrl;
configData.translatedStrings.languageError;
configData.translatedStrings.learnMore;
configData.translatedStrings.linkAddedMsg;
configData.translatedStrings.linkCancel;
configData.translatedStrings.linkCantCreate;
configData.translatedStrings.linkCantEdit;
configData.translatedStrings.linkCantRemove;
configData.translatedStrings.linkDone;
configData.translatedStrings.linkEditedMsg;
configData.translatedStrings.linkRemove;
configData.translatedStrings.linkRemovedMsg;
configData.translatedStrings.listening;
configData.translatedStrings.moreInfo;
configData.translatedStrings.mostVisitedTitle;
configData.translatedStrings.nameField;
configData.translatedStrings.networkError;
configData.translatedStrings.noTranslation;
configData.translatedStrings.noVoice;
configData.translatedStrings.otherError;
configData.translatedStrings.permissionError;
configData.translatedStrings.ready;
configData.translatedStrings.removeThumbnailTooltip;
configData.translatedStrings.restoreDefaultBackground;
configData.translatedStrings.restoreDefaultLinks;
configData.translatedStrings.restoreThumbnailsShort;
configData.translatedStrings.searchboxPlaceholder;
configData.translatedStrings.selectChromeWallpaper;
configData.translatedStrings.selectedLabel;
configData.translatedStrings.selectionCancel;
configData.translatedStrings.selectionDone;
configData.translatedStrings.shareClose;
configData.translatedStrings.shareDoodle;
configData.translatedStrings.shareFacebook;
configData.translatedStrings.shareLink;
configData.translatedStrings.shareMail;
configData.translatedStrings.shareTwitter;
configData.translatedStrings.thumbnailRemovedNotification;
configData.translatedStrings.title;
configData.translatedStrings.tryAgain;
configData.translatedStrings.undoThumbnailRemove;
configData.translatedStrings.uploadImage;
configData.translatedStrings.urlField;
configData.translatedStrings.voiceCloseTooltip;
configData.translatedStrings.waiting;
