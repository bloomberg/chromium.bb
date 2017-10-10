// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Runs the Polymer Settings tests. */

/** @const {string} Path to source root. */
var ROOT_PATH = '../../../../../';

// Polymer BrowserTest fixture.
GEN_INCLUDE([
    ROOT_PATH + 'chrome/test/data/webui/polymer_browser_test_base.js']);
GEN('#include "base/command_line.h"');
GEN('#include "chrome/browser/ui/webui/extensions/' +
    'extension_settings_browsertest.h"');

/**
 * Basic test fixture for the MD chrome://extensions page. Installs no
 * extensions.
 */
var CrExtensionsBrowserTest = class extends PolymerTest {
  /** @override */
  get browsePreload() {
    return 'chrome://extensions/';
  }

  /** @override */
  get commandLineSwitches() {
    return [{
      switchName: 'enable-features',
      switchValue: 'MaterialDesignExtensions',
    }];
  }

  /** @override */
  get extraLibraries() {
    return PolymerTest.getLibraries(ROOT_PATH).concat([
      ROOT_PATH + 'ui/webui/resources/js/assert.js',
      'extension_test_util.js',
      '../mock_controller.js',
      '../../../../../ui/webui/resources/js/promise_resolver.js',
      '../../../../../ui/webui/resources/js/webui_resource_test.js',
    ]);
  }

  /** @override */
  get typedefCppFixture() {
    return 'ExtensionSettingsUIBrowserTest';
  }
};

/**
 * Test fixture with one installed extension.
 */
var CrExtensionsBrowserTestWithInstalledExtension =
    class extends CrExtensionsBrowserTest {
  /** @override */
  testGenPreamble() {
    GEN('  InstallGoodExtension();');
    GEN('  SetAutoConfirmUninstall();');
  }
};

////////////////////////////////////////////////////////////////////////////////
// Extension Sidebar Tests

var CrExtensionsSidebarTest = class extends CrExtensionsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://extensions/sidebar.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      'extension_sidebar_test.js',
    ]);
  }
};

TEST_F('CrExtensionsSidebarTest', 'LayoutAndClickHandlers', function() {
  mocha.grep(assert(extension_sidebar_tests.TestNames.LayoutAndClickHandlers))
      .run();
});

TEST_F('CrExtensionsSidebarTest', 'UpdateSelected', function() {
  mocha.grep(assert(extension_sidebar_tests.TestNames.UpdateSelected)).run();
});

////////////////////////////////////////////////////////////////////////////////
// Extension Toolbar Tests

var CrExtensionsToolbarTest = class extends CrExtensionsBrowserTest {
  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      'extension_toolbar_test.js',
    ]);
  }
};

TEST_F('CrExtensionsToolbarTest', 'Layout', function() {
  mocha.grep(assert(extension_toolbar_tests.TestNames.Layout)).run();
});

TEST_F('CrExtensionsToolbarTest', 'ClickHandlers', function() {
  mocha.grep(assert(extension_toolbar_tests.TestNames.ClickHandlers)).run();
});

////////////////////////////////////////////////////////////////////////////////
// Extension Item Tests

var CrExtensionsItemsTest = class extends CrExtensionsBrowserTest {
  get browsePreload() {
    return 'chrome://extensions/item.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      'extension_item_test.js',
    ]);
  }
};

TEST_F('CrExtensionsItemsTest', 'NormalState', function() {
  var TestNames = extension_item_tests.TestNames;
  mocha.grep(assert(TestNames.ElementVisibilityNormalState)).run();
});

TEST_F('CrExtensionsItemsTest', 'DeveloperState', function() {
  var TestNames = extension_item_tests.TestNames;
  mocha.grep(assert(TestNames.ElementVisibilityDeveloperState)).run();
});

TEST_F('CrExtensionsItemsTest', 'ClickableItems', function() {
  var TestNames = extension_item_tests.TestNames;
  mocha.grep(assert(TestNames.ClickableItems)).run();
});

TEST_F('CrExtensionsItemsTest', 'Warnings', function() {
  mocha.grep(assert(extension_item_tests.TestNames.Warnings)).run();
});

TEST_F('CrExtensionsItemsTest', 'SourceIndicator', function() {
  mocha.grep(assert(extension_item_tests.TestNames.SourceIndicator)).run();
});

TEST_F('CrExtensionsItemsTest', 'EnableToggle', function() {
  mocha.grep(assert(extension_item_tests.TestNames.EnableToggle)).run();
});

TEST_F('CrExtensionsItemsTest', 'RemoveButton', function() {
  mocha.grep(assert(extension_item_tests.TestNames.RemoveButton)).run();
});

////////////////////////////////////////////////////////////////////////////////
// Extension Detail View Tests

var CrExtensionsDetailViewTest = class extends CrExtensionsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://extensions/detail_view.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      'extension_detail_view_test.js',
    ]);
  }
};

TEST_F('CrExtensionsDetailViewTest', 'Layout', function() {
  mocha.grep(assert(extension_detail_view_tests.TestNames.Layout)).run();
});

TEST_F(
    'CrExtensionsDetailViewTest', 'ClickableElements', function() {
      mocha
          .grep(assert(extension_detail_view_tests.TestNames.ClickableElements))
          .run();
    });

TEST_F(
    'CrExtensionsDetailViewTest', 'IndicatorTest',
    function() {
      mocha
          .grep(assert(extension_detail_view_tests.TestNames.Indicator))
          .run();
    });

TEST_F(
    'CrExtensionsDetailViewTest', 'Warnings',
    function() {
      mocha
          .grep(assert(extension_detail_view_tests.TestNames.Warnings))
          .run();
    });

////////////////////////////////////////////////////////////////////////////////
// Extension Item List Tests

var CrExtensionsItemListTest = class extends CrExtensionsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://extensions/item_list.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      'extension_item_list_test.js',
    ]);
  }
};

// This test is flaky on Mac10.9 Tests (dbg). See https://crbug.com/771099.
GEN('#if defined(OS_MACOSX)');
GEN('#define MAYBE_Filtering DISABLED_Filtering');
GEN('#else');
GEN('#define MAYBE_Filtering Filtering');
GEN('#endif');
TEST_F('CrExtensionsItemListTest', 'MAYBE_Filtering', function() {
  mocha.grep(assert(extension_item_list_tests.TestNames.Filtering)).run();
});

// This test is flaky on Mac10.9 Tests (dbg). See https://crbug.com/771099.
GEN('#if defined(OS_MACOSX)');
GEN('#define MAYBE_NoItems DISABLED_NoItems');
GEN('#else');
GEN('#define MAYBE_NoItems NoItems');
GEN('#endif');
TEST_F('CrExtensionsItemListTest', 'MAYBE_NoItems', function() {
  mocha.grep(assert(extension_item_list_tests.TestNames.NoItemsMsg)).run();
});

TEST_F('CrExtensionsItemListTest', 'NoSearchResults', function() {
  mocha.grep(assert(extension_item_list_tests.TestNames.NoSearchResultsMsg))
      .run();
});

////////////////////////////////////////////////////////////////////////////////
// Extension Load Error Tests

var CrExtensionsLoadErrorTest = class extends CrExtensionsBrowserTest {
  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      'extension_load_error_test.js',
    ]);
  }
};

TEST_F('CrExtensionsLoadErrorTest', 'Interaction', function() {
  mocha.grep(assert(extension_load_error_tests.TestNames.Interaction)).run();
});

TEST_F('CrExtensionsLoadErrorTest', 'CodeSection', function() {
  mocha.grep(assert(extension_load_error_tests.TestNames.CodeSection)).run();
});

////////////////////////////////////////////////////////////////////////////////
// Extension Service Tests

var CrExtensionsServiceTest =
    class extends CrExtensionsBrowserTestWithInstalledExtension {
  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      'extension_service_test.js',
    ]);
  }
};

TEST_F('CrExtensionsServiceTest', 'ToggleEnable', function() {
  mocha.grep(assert(extension_service_tests.TestNames.EnableAndDisable)).run();
});

TEST_F(
    'CrExtensionsServiceTest', 'ToggleIncognito', function() {
      mocha.grep(assert(extension_service_tests.TestNames.ToggleIncognitoMode))
          .run();
    });

TEST_F('CrExtensionsServiceTest', 'Uninstall', function() {
  mocha.grep(assert(extension_service_tests.TestNames.Uninstall)).run();
});

TEST_F('CrExtensionsServiceTest', 'ProfileSettings', function() {
  mocha.grep(assert(extension_service_tests.TestNames.ProfileSettings)).run();
});

////////////////////////////////////////////////////////////////////////////////
// Extension Manager Tests

var CrExtensionsManagerTest = class extends CrExtensionsBrowserTest {
  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      'extension_manager_test.js',
    ]);
  }
};

var CrExtensionsManagerTestWithMultipleExtensionTypesInstalled =
    class extends CrExtensionsBrowserTest {
  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      'extension_manager_test.js',
    ]);
  }

  /** @override */
  testGenPreamble() {
    GEN('  InstallGoodExtension();');
    GEN('  InstallPackagedApp();');
    GEN('  InstallHostedApp();');
    GEN('  InstallPlatformApp();');
  }
};

var CrExtensionsManagerTestWithIdQueryParam =
    class extends CrExtensionsBrowserTestWithInstalledExtension {
  /** @override */
  get browsePreload() {
    return 'chrome://extensions/?id=ldnnhddmnhbkjipkidpdiheffobcpfmf';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      'extension_manager_test.js',
    ]);
  }
};

TEST_F('CrExtensionsManagerTest', 'ItemOrder', function() {
  mocha.grep(assert(extension_manager_tests.TestNames.ItemOrder)).run();
});

TEST_F(
    'CrExtensionsManagerTestWithMultipleExtensionTypesInstalled',
    'ItemListVisibility', function() {
      mocha.grep(assert(extension_manager_tests.TestNames.ItemListVisibility))
          .run();
    });

TEST_F(
    'CrExtensionsManagerTestWithMultipleExtensionTypesInstalled', 'ShowItems',
    function() {
      mocha.grep(assert(extension_manager_tests.TestNames.ShowItems)).run();
    });

TEST_F(
    'CrExtensionsManagerTestWithMultipleExtensionTypesInstalled', 'ChangePages',
    function() {
      mocha.grep(assert(extension_manager_tests.TestNames.ChangePages)).run();
    });

TEST_F(
    'CrExtensionsManagerTestWithMultipleExtensionTypesInstalled',
    'SidebarHighlighting', function() {
      mocha.grep(assert(extension_manager_tests.TestNames.SidebarHighlighting))
          .run();
    });

TEST_F(
    'CrExtensionsManagerTestWithIdQueryParam', 'NavigationToDetails',
    function() {
      mocha
          .grep(
              assert(extension_manager_tests.TestNames.UrlNavigationToDetails))
          .run();
    });

TEST_F('CrExtensionsManagerTest', 'UpdateItemData', function() {
  mocha.grep(assert(extension_manager_tests.TestNames.UpdateItemData)).run();
});

////////////////////////////////////////////////////////////////////////////////
// Extension Keyboard Shortcuts Tests

var CrExtensionsShortcutTest = class extends CrExtensionsBrowserTest {
  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      'extension_keyboard_shortcuts_test.js',
      'extension_shortcut_input_test.js',
    ]);
  }
};

TEST_F('CrExtensionsShortcutTest', 'Layout', function() {
  mocha.grep(assert(extension_keyboard_shortcut_tests.TestNames.Layout)).run();
});

TEST_F('CrExtensionsShortcutTest', 'Util', function() {
  mocha.grep(
      assert(extension_keyboard_shortcut_tests.TestNames.ShortcutUtil)).run();
});

TEST_F('CrExtensionsShortcutTest', 'Basic', function() {
  mocha.grep(
      assert(extension_shortcut_input_tests.TestNames.Basic)).run();
});

////////////////////////////////////////////////////////////////////////////////
// Extension Pack Dialog Tests

var CrExtensionsPackDialogTest = class extends CrExtensionsBrowserTest {
  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      'extension_pack_dialog_test.js',
    ]);
  }
};

TEST_F('CrExtensionsPackDialogTest', 'Interaction', function() {
  mocha.grep(assert(extension_pack_dialog_tests.TestNames.Interaction)).run();
});

TEST_F('CrExtensionsPackDialogTest', 'PackSuccess', function() {
  mocha.grep(assert(extension_pack_dialog_tests.TestNames.PackSuccess)).run();
});

TEST_F('CrExtensionsPackDialogTest', 'PackError', function() {
  mocha.grep(assert(extension_pack_dialog_tests.TestNames.PackError)).run();
});

TEST_F('CrExtensionsPackDialogTest', 'PackWarning', function() {
  mocha.grep(assert(extension_pack_dialog_tests.TestNames.PackWarning)).run();
});

////////////////////////////////////////////////////////////////////////////////
// Extension Options Dialog Tests

var CrExtensionsOptionsDialogTest = class extends CrExtensionsBrowserTest {
  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      'extension_options_dialog_test.js',
    ]);
  }
};

TEST_F('CrExtensionsOptionsDialogTest', 'Layout', function() {
  mocha.grep(assert(extension_options_dialog_tests.TestNames.Layout)).run();
});

////////////////////////////////////////////////////////////////////////////////
// Extension Error Page Tests

var CrExtensionsErrorPageTest = class extends CrExtensionsBrowserTest {
  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      'extension_error_page_test.js',
    ]);
  }
};

TEST_F('CrExtensionsErrorPageTest', 'Layout', function() {
  mocha.grep(assert(extension_error_page_tests.TestNames.Layout)).run();
});

TEST_F('CrExtensionsErrorPageTest', 'CodeSection', function() {
  mocha.grep(assert(extension_error_page_tests.TestNames.CodeSection)).run();
});

TEST_F('CrExtensionsErrorPageTest', 'ErrorSelection', function() {
  mocha.grep(assert(extension_error_page_tests.TestNames.ErrorSelection)).run();
});

////////////////////////////////////////////////////////////////////////////////
// Extension Code Section Tests

var CrExtensionsCodeSectionTest = class extends CrExtensionsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://extensions/code_section.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      'extension_code_section_test.js',
    ]);
  }
};

TEST_F('CrExtensionsCodeSectionTest', 'Layout', function() {
  mocha.grep(assert(extension_code_section_tests.TestNames.Layout)).run();
});

////////////////////////////////////////////////////////////////////////////////
// Extension Navigation Helper Tests

var CrExtensionsNavigationHelperTest = class extends CrExtensionsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://extensions/navigation_helper.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      'extension_navigation_helper_test.js',
    ]);
  }
};

TEST_F('CrExtensionsNavigationHelperTest', 'Basic', function() {
  mocha.grep(assert(extension_navigation_helper_tests.TestNames.Basic)).run();
});

TEST_F('CrExtensionsNavigationHelperTest', 'Conversion', function() {
  mocha.grep(
      assert(extension_navigation_helper_tests.TestNames.Conversions)).run();
});

TEST_F('CrExtensionsNavigationHelperTest', 'PushAndReplaceState', function() {
  mocha
      .grep(assert(
          extension_navigation_helper_tests.TestNames.PushAndReplaceState))
      .run();
});

////////////////////////////////////////////////////////////////////////////////
// Extension View Manager Tests

var CrExtensionsViewManagerTest = class extends CrExtensionsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://extensions/view_manager.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      'extension_view_manager_test.js',
    ]);
  }
}

TEST_F('CrExtensionsViewManagerTest', 'VisibilityTest', function() {
  mocha.grep(assert(extension_view_manager_tests.TestNames.Visibility)).run();
});

TEST_F('CrExtensionsViewManagerTest', 'EventFiringTest', function() {
  mocha.grep(assert(extension_view_manager_tests.TestNames.EventFiring)).run();
});
