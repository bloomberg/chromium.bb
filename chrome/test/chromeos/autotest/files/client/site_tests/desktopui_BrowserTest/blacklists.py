# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# TODO(rkc): Tests currently failing on the PFQ - resolve these.
# http://code.google.com/p/chromium-os/issues/detail?id=20128
blacklist = ['FindInPageControllerTest.AcceleratorRestoring',
             'FindInPageControllerTest.FindMovesWhenObscuring',
             'FindInPageControllerTest.FitWindow',
             'BrowserNavigatorTest.NavigateFromBlankToOptionsInSameTab',
             'BrowserNavigatorTest.NavigateFromOmniboxIntoNewTab',
             'PrerenderBrowserTest.PrerenderDelayLoadPlugin',
             'PrerenderBrowserTest.PrerenderIframeDelayLoadPlugin',
             'PrerenderBrowserTest.PrerenderHTML5Audio',
             'PrerenderBrowserTest.PrerenderHTML5Video',
             'PrerenderBrowserTest.PrerenderHTML5VideoJs',
             'NaClExtensionTest.WebStoreExtension',
             'NaClExtensionTest.ComponentExtension',
             'NaClExtensionTest.UnpackedExtension',
             'ExtensionResourceRequestPolicyTest.Audio',
             'ExtensionResourceRequestPolicyTest.Video',
             'ExtensionCrashRecoveryTest.Basic',
             'ExtensionCrashRecoveryTest.CloseAndReload',
             'ExtensionCrashRecoveryTest.ReloadIndependentlyChangeTabs',
             'ExtensionCrashRecoveryTest.ReloadIndependentlyNavigatePage',
             'ExtensionCrashRecoveryTest.TwoExtensionsCrashFirst',
             'ExtensionCrashRecoveryTest.TwoExtensionsCrashSecond',
             'ExtensionCrashRecoveryTest.TwoExtensionsCrashBothAtOnce',
             'ExtensionCrashRecoveryTest.TwoExtensionsOneByOne',
             'ExtensionCrashRecoveryTest.TwoExtensionsReloadIndependently',
             'ExtensionCrashRecoveryTest.CrashAndUninstall',
             'ExtensionApiTest.WebNavigationUserAction',
             'ExistingUserControllerTest.NewUserLogin',
             'OmniboxApiTest.PopupStaysClosed']


blacklist_vm = ['GPUBrowserTest.CanOpenPopupAndRenderWithWebGLCanvas']
