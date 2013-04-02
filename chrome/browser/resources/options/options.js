// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var AddLanguageOverlay = options.AddLanguageOverlay;
var AlertOverlay = options.AlertOverlay;
var AutofillEditAddressOverlay = options.AutofillEditAddressOverlay;
var AutofillEditCreditCardOverlay = options.AutofillEditCreditCardOverlay;
var AutofillOptions = options.AutofillOptions;
var BrowserOptions = options.BrowserOptions;
var ClearBrowserDataOverlay = options.ClearBrowserDataOverlay;
var ConfirmDialog = options.ConfirmDialog;
var ContentSettingsExceptionsArea =
    options.contentSettings.ContentSettingsExceptionsArea;
var ContentSettings = options.ContentSettings;
var CookiesView = options.CookiesView;
var CreateProfileOverlay = options.CreateProfileOverlay;
var EditDictionaryOverlay = cr.IsMac ? null : options.EditDictionaryOverlay;
var FactoryResetOverlay = options.FactoryResetOverlay;
var ManagedUserSetPassphraseOverlay = options.ManagedUserSetPassphraseOverlay;
var ManagedUserSettings = options.ManagedUserSettings;
var FontSettings = options.FontSettings;
var HandlerOptions = options.HandlerOptions;
var HomePageOverlay = options.HomePageOverlay;
var ImportDataOverlay = options.ImportDataOverlay;
var LanguageOptions = options.LanguageOptions;
var ManageProfileOverlay = options.ManageProfileOverlay;
var MediaGalleriesManager = options.MediaGalleriesManager;
var OptionsFocusManager = options.OptionsFocusManager;
var OptionsPage = options.OptionsPage;
var PasswordManager = options.PasswordManager;
var Preferences = options.Preferences;
var PreferredNetworks = options.PreferredNetworks;
var SearchEngineManager = options.SearchEngineManager;
var SearchPage = options.SearchPage;
var StartupOverlay = options.StartupOverlay;
var SyncSetupOverlay = options.SyncSetupOverlay;

/**
 * DOMContentLoaded handler, sets up the page.
 */
function load() {
  // Decorate the existing elements in the document.
  cr.ui.decorate('input[pref][type=checkbox]', options.PrefCheckbox);
  cr.ui.decorate('input[pref][type=number]', options.PrefNumber);
  cr.ui.decorate('input[pref][type=radio]', options.PrefRadio);
  cr.ui.decorate('input[pref][type=range]', options.PrefRange);
  cr.ui.decorate('select[pref]', options.PrefSelect);
  cr.ui.decorate('input[pref][type=text]', options.PrefTextField);
  cr.ui.decorate('input[pref][type=url]', options.PrefTextField);
  cr.ui.decorate('button[pref]', options.PrefButton);
  cr.ui.decorate('#content-settings-page input[type=radio]:not(.handler-radio)',
      options.ContentSettingsRadio);
  cr.ui.decorate('#content-settings-page input[type=radio].handler-radio',
      options.HandlersEnabledRadio);
  cr.ui.decorate('span.controlled-setting-indicator',
      options.ControlledSettingIndicator);

  // Top level pages.
  OptionsPage.register(SearchPage.getInstance());
  OptionsPage.register(BrowserOptions.getInstance());

  // Overlays.
  OptionsPage.registerOverlay(AddLanguageOverlay.getInstance(),
                              LanguageOptions.getInstance());
  OptionsPage.registerOverlay(AlertOverlay.getInstance());
  OptionsPage.registerOverlay(AutofillEditAddressOverlay.getInstance(),
                              AutofillOptions.getInstance());
  OptionsPage.registerOverlay(AutofillEditCreditCardOverlay.getInstance(),
                              AutofillOptions.getInstance());
  OptionsPage.registerOverlay(AutofillOptions.getInstance(),
                              BrowserOptions.getInstance(),
                              [$('autofill-settings')]);
  OptionsPage.registerOverlay(ClearBrowserDataOverlay.getInstance(),
                              BrowserOptions.getInstance(),
                              [$('privacyClearDataButton')]);
  OptionsPage.registerOverlay(
      new ConfirmDialog(
          'doNotTrackConfirm',
          loadTimeData.getString('doNotTrackConfirmOverlayTabTitle'),
          'do-not-track-confirm-overlay',
          $('do-not-track-confirm-ok'),
          $('do-not-track-confirm-cancel'),
          $('do-not-track-enabled').pref,
          $('do-not-track-enabled').metric),
      BrowserOptions.getInstance());
  OptionsPage.registerOverlay(
      new ConfirmDialog(
          'instantConfirm',
          loadTimeData.getString('instantConfirmOverlayTabTitle'),
          'instantConfirmOverlay',
          $('instantConfirmOk'),
          $('instantConfirmCancel'),
          $('instant-enabled-control').pref,
          $('instant-enabled-control').metric,
          'instant.confirm_dialog_shown'),
      BrowserOptions.getInstance());
  // 'spelling-enabled-control' element is only present on Chrome branded
  // builds.
  if ($('spelling-enabled-control')) {
    OptionsPage.registerOverlay(
        new ConfirmDialog(
            'spellingConfirm',
            loadTimeData.getString('spellingConfirmOverlayTabTitle'),
            'spelling-confirm-overlay',
            $('spelling-confirm-ok'),
            $('spelling-confirm-cancel'),
            $('spelling-enabled-control').pref,
            $('spelling-enabled-control').metric,
            'spellcheck.confirm_dialog_shown'),
        BrowserOptions.getInstance());
  }
  OptionsPage.registerOverlay(ContentSettings.getInstance(),
                              BrowserOptions.getInstance(),
                              [$('privacyContentSettingsButton')]);
  OptionsPage.registerOverlay(ContentSettingsExceptionsArea.getInstance(),
                              ContentSettings.getInstance());
  OptionsPage.registerOverlay(CookiesView.getInstance(),
                              ContentSettings.getInstance(),
                              [$('privacyContentSettingsButton'),
                               $('show-cookies-button')]);
  OptionsPage.registerOverlay(CreateProfileOverlay.getInstance(),
                              BrowserOptions.getInstance());
  if (!cr.isMac) {
    OptionsPage.registerOverlay(EditDictionaryOverlay.getInstance(),
                                LanguageOptions.getInstance(),
                                [$('edit-dictionary-button')]);
  }
  OptionsPage.registerOverlay(FontSettings.getInstance(),
                              BrowserOptions.getInstance(),
                              [$('fontSettingsCustomizeFontsButton')]);
  if (HandlerOptions && $('manage-handlers-button')) {
    OptionsPage.registerOverlay(HandlerOptions.getInstance(),
                                ContentSettings.getInstance(),
                                [$('manage-handlers-button')]);
  }
  OptionsPage.registerOverlay(HomePageOverlay.getInstance(),
                              BrowserOptions.getInstance(),
                              [$('change-home-page')]);
  OptionsPage.registerOverlay(ImportDataOverlay.getInstance(),
                              BrowserOptions.getInstance());
  OptionsPage.registerOverlay(LanguageOptions.getInstance(),
                              BrowserOptions.getInstance(),
                              [$('language-button')]);
  OptionsPage.registerOverlay(ManageProfileOverlay.getInstance(),
                              BrowserOptions.getInstance());
  if (loadTimeData.getBoolean('managedUsersEnabled')) {
    OptionsPage.registerOverlay(ManagedUserSetPassphraseOverlay.getInstance(),
                                ManagedUserSettings.getInstance(),
                                [$('set-passphrase')]);
    OptionsPage.registerOverlay(ManagedUserSettings.getInstance(),
                                BrowserOptions.getInstance(),
                                [$('managed-user-settings-button')]);
  }
  OptionsPage.registerOverlay(MediaGalleriesManager.getInstance(),
                              ContentSettings.getInstance(),
                              [$('manage-galleries-button')]);
  OptionsPage.registerOverlay(PasswordManager.getInstance(),
                              BrowserOptions.getInstance(),
                              [$('manage-passwords')]);
  OptionsPage.registerOverlay(SearchEngineManager.getInstance(),
                              BrowserOptions.getInstance(),
                              [$('manage-default-search-engines')]);
  OptionsPage.registerOverlay(StartupOverlay.getInstance(),
                              BrowserOptions.getInstance());
  OptionsPage.registerOverlay(SyncSetupOverlay.getInstance(),
                              BrowserOptions.getInstance(),
                              [$('customize-sync')]);
  if (cr.isChromeOS) {
    OptionsPage.registerOverlay(AccountsOptions.getInstance(),
                                BrowserOptions.getInstance(),
                                [$('manage-accounts-button')]);
    OptionsPage.registerOverlay(BluetoothOptions.getInstance(),
                                BrowserOptions.getInstance(),
                                [$('bluetooth-add-device')]);
    OptionsPage.registerOverlay(BluetoothPairing.getInstance(),
                                BrowserOptions.getInstance());
    OptionsPage.registerOverlay(FactoryResetOverlay.getInstance(),
                                BrowserOptions.getInstance(),
                                [$('factory-reset-restart')]);
    OptionsPage.registerOverlay(ChangePictureOptions.getInstance(),
                                BrowserOptions.getInstance(),
                                [$('account-picture')]);
    OptionsPage.registerOverlay(DetailsInternetPage.getInstance(),
                                BrowserOptions.getInstance());
    OptionsPage.registerOverlay(DisplayOptions.getInstance(),
                                BrowserOptions.getInstance(),
                                [$('display-options')]);
    OptionsPage.registerOverlay(DisplayOverscan.getInstance(),
                                DisplayOptions.getInstance());
    OptionsPage.registerOverlay(KeyboardOverlay.getInstance(),
                                BrowserOptions.getInstance(),
                                [$('keyboard-settings-button')]);
    OptionsPage.registerOverlay(KioskAppsOverlay.getInstance(),
                                BrowserOptions.getInstance(),
                                [$('manage-kiosk-apps-button')]);
    OptionsPage.registerOverlay(KioskDisableBailoutConfirm.getInstance(),
                                KioskAppsOverlay.getInstance());
    OptionsPage.registerOverlay(PointerOverlay.getInstance(),
                                BrowserOptions.getInstance(),
                                [$('pointer-settings-button')]);
    OptionsPage.registerOverlay(PreferredNetworks.getInstance(),
                                BrowserOptions.getInstance());
    OptionsPage.registerOverlay(
        new OptionsPage('languageChewing',
                        loadTimeData.getString('languageChewingPageTabTitle'),
                        'languageChewingPage'),
        LanguageOptions.getInstance());
    OptionsPage.registerOverlay(
        new OptionsPage('languageHangul',
                        loadTimeData.getString('languageHangulPageTabTitle'),
                        'languageHangulPage'),
        LanguageOptions.getInstance());
    OptionsPage.registerOverlay(
        new OptionsPage('languageMozc',
                        loadTimeData.getString('languageMozcPageTabTitle'),
                        'languageMozcPage'),
        LanguageOptions.getInstance());
    OptionsPage.registerOverlay(
        new OptionsPage('languagePinyin',
                        loadTimeData.getString('languagePinyinPageTabTitle'),
                        'languagePinyinPage'),
        LanguageOptions.getInstance());
  }

  if (!cr.isWindows && !cr.isMac) {
    OptionsPage.registerOverlay(CertificateBackupOverlay.getInstance(),
                                CertificateManager.getInstance());
    OptionsPage.registerOverlay(CertificateEditCaTrustOverlay.getInstance(),
                                CertificateManager.getInstance());
    OptionsPage.registerOverlay(CertificateImportErrorOverlay.getInstance(),
                                CertificateManager.getInstance());
    OptionsPage.registerOverlay(CertificateManager.getInstance(),
                                BrowserOptions.getInstance(),
                                [$('certificatesManageButton')]);
    OptionsPage.registerOverlay(CertificateRestoreOverlay.getInstance(),
                                CertificateManager.getInstance());
  }

  OptionsFocusManager.getInstance().initialize();
  Preferences.getInstance().initialize();
  OptionsPage.initialize();

  var path = document.location.pathname;

  if (path.length > 1) {
    // Skip starting slash and remove trailing slash (if any).
    var pageName = path.slice(1).replace(/\/$/, '');
    OptionsPage.showPageByName(pageName, true, {replaceState: true});
  } else {
    OptionsPage.showDefaultPage();
  }

  var subpagesNavTabs = document.querySelectorAll('.subpages-nav-tabs');
  for (var i = 0; i < subpagesNavTabs.length; i++) {
    subpagesNavTabs[i].onclick = function(event) {
      OptionsPage.showTab(event.srcElement);
    };
  }

  if (navigator.plugins['Shockwave Flash'])
    document.documentElement.setAttribute('hasFlashPlugin', '');

  window.setTimeout(function() {
    document.documentElement.classList.remove('loading');
  });
}

document.documentElement.classList.add('loading');
document.addEventListener('DOMContentLoaded', load);

/**
 * Listener for the |beforeunload| event.
 */
window.onbeforeunload = function() {
  options.OptionsPage.willClose();
};

/**
 * Listener for the |popstate| event.
 * @param {Event} e The |popstate| event.
 */
window.onpopstate = function(e) {
  options.OptionsPage.setState(e.state);
};
