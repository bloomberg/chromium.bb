// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/use_counter_page_load_metrics_observer.h"

#include "base/no_destructor.h"

// This file defines a list of UseCounter WebFeature measured in the
// UKM-based UseCounter. Features must all satisfy UKM privacy requirements
// (see go/ukm). In addition, features should only be added if it's shown
// (or highly likely be) rare, e.g. <1% of page views as measured by UMA.
//
// UKM-based UseCounter should be used to cover the case when UMA UseCounter
// data shows a behaviour that is rare but too common to bindly change.
// UKM-based UseCounter would allow use to find specific pages to reason about
// either a breaking change is acceptable or not.

using WebFeature = blink::mojom::WebFeature;

// UKM-based UseCounter features (WebFeature) should be defined in
// opt_in_features list.
const UseCounterPageLoadMetricsObserver::UkmFeatureList&
UseCounterPageLoadMetricsObserver::GetAllowedUkmFeatures() {
  static base::NoDestructor<UseCounterPageLoadMetricsObserver::UkmFeatureList>
      // We explicitly use an std::initializer_list below to work around GCC
      // bug 84849, which causes having a base::NoDestructor<T<U>> and passing
      // an initializer list of Us does not work.
      opt_in_features(std::initializer_list<WebFeature>({
          WebFeature::kNavigatorVibrate,
          WebFeature::kNavigatorVibrateSubFrame,
          WebFeature::kTouchEventPreventedNoTouchAction,
          WebFeature::kTouchEventPreventedForcedDocumentPassiveNoTouchAction,
          // kDataUriHasOctothorpe may not be recorded correctly for iframes.
          // See https://crbug.com/796173 for details.
          WebFeature::kDataUriHasOctothorpe,
          WebFeature::kApplicationCacheManifestSelectInsecureOrigin,
          WebFeature::kApplicationCacheManifestSelectSecureOrigin,
          WebFeature::kMixedContentAudio,
          WebFeature::kMixedContentImage,
          WebFeature::kMixedContentVideo,
          WebFeature::kMixedContentPlugin,
          WebFeature::kOpenerNavigationWithoutGesture,
          WebFeature::kUsbRequestDevice,
          WebFeature::kXMLHttpRequestSynchronous,
          WebFeature::kPaymentHandler,
          WebFeature::kPaymentRequestShowWithoutGesture,
          WebFeature::kHTMLImports,
          WebFeature::kHTMLImportsHasStyleSheets,
          WebFeature::kElementCreateShadowRoot,
          WebFeature::kDocumentRegisterElement,
          WebFeature::kCredentialManagerCreatePublicKeyCredential,
          WebFeature::kCredentialManagerGetPublicKeyCredential,
          WebFeature::kCredentialManagerMakePublicKeyCredentialSuccess,
          WebFeature::kCredentialManagerGetPublicKeyCredentialSuccess,
          WebFeature::kV8AudioContext_Constructor,
          WebFeature::kElementAttachShadow,
          WebFeature::kElementAttachShadowOpen,
          WebFeature::kElementAttachShadowClosed,
          WebFeature::kCustomElementRegistryDefine,
          WebFeature::kTextToSpeech_Speak,
          WebFeature::kTextToSpeech_SpeakDisallowedByAutoplay,
          WebFeature::kCSSEnvironmentVariable,
          WebFeature::kCSSEnvironmentVariable_SafeAreaInsetTop,
          WebFeature::kCSSEnvironmentVariable_SafeAreaInsetLeft,
          WebFeature::kCSSEnvironmentVariable_SafeAreaInsetRight,
          WebFeature::kCSSEnvironmentVariable_SafeAreaInsetBottom,
          WebFeature::kMediaControlsDisplayCutoutGesture,
          WebFeature::kPolymerV1Detected,
          WebFeature::kPolymerV2Detected,
          WebFeature::kFullscreenSecureOrigin,
          WebFeature::kFullscreenInsecureOrigin,
          WebFeature::kPrefixedVideoEnterFullscreen,
          WebFeature::kPrefixedVideoExitFullscreen,
          WebFeature::kPrefixedVideoEnterFullScreen,
          WebFeature::kPrefixedVideoExitFullScreen,
          WebFeature::kDocumentLevelPassiveDefaultEventListenerPreventedWheel,
          WebFeature::kDocumentDomainBlockedCrossOriginAccess,
          WebFeature::kDocumentDomainEnabledCrossOriginAccess,
          WebFeature::kSuppressHistoryEntryWithoutUserGesture,
          WebFeature::kCursorImageGT32x32,
          WebFeature::kCursorImageLE32x32,
          WebFeature::kHistoryPushState,
          WebFeature::kHistoryReplaceState,
          WebFeature::kCursorImageGT64x64,
          WebFeature::kAdClick,
          WebFeature::kUpdateWithoutShippingOptionOnShippingAddressChange,
          WebFeature::kUpdateWithoutShippingOptionOnShippingOptionChange,
          WebFeature::kSignedExchangeInnerResponseInMainFrame,
          WebFeature::kSignedExchangeInnerResponseInSubFrame,
          WebFeature::kWebShareShare,
          WebFeature::kHTMLAnchorElementDownloadInSandboxWithUserGesture,
          WebFeature::kHTMLAnchorElementDownloadInSandboxWithoutUserGesture,
          WebFeature::kNavigationDownloadInSandboxWithUserGesture,
          WebFeature::kNavigationDownloadInSandboxWithoutUserGesture,
          WebFeature::kDownloadInAdFrameWithUserGesture,
          WebFeature::kDownloadInAdFrameWithoutUserGesture,
          WebFeature::kOpenWebDatabase,
          WebFeature::kV8MediaCapabilities_DecodingInfo_Method,
          WebFeature::kOpenerNavigationDownloadCrossOriginNoGesture,
          WebFeature::kLinkRelPrerender,
      }));
  return *opt_in_features;
}
