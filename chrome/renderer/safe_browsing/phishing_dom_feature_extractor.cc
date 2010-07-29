// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/safe_browsing/phishing_dom_feature_extractor.h"

#include "base/compiler_specific.h"
#include "base/hash_tables.h"
#include "base/histogram.h"
#include "base/logging.h"
#include "chrome/renderer/render_view.h"
#include "chrome/renderer/safe_browsing/features.h"
#include "net/base/registry_controlled_domain.h"
#include "third_party/WebKit/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/WebKit/chromium/public/WebElement.h"
#include "third_party/WebKit/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/WebKit/chromium/public/WebNodeCollection.h"
#include "third_party/WebKit/WebKit/chromium/public/WebString.h"
#include "third_party/WebKit/WebKit/chromium/public/WebView.h"

namespace safe_browsing {

// Intermediate state used for computing features.  See features.h for
// descriptions of the DOM features that are computed.
struct PhishingDOMFeatureExtractor::PageFeatureState {
  // Link related features
  int external_links;
  base::hash_set<std::string> external_domains;
  int secure_links;
  int total_links;

  // Form related features
  int num_forms;
  int num_text_inputs;
  int num_pswd_inputs;
  int num_radio_inputs;
  int num_check_inputs;
  int action_other_domain;
  int total_actions;

  // Image related features
  int img_other_domain;
  int total_imgs;

  // How many script tags
  int num_script_tags;

  PageFeatureState()
      : external_links(0),
        secure_links(0),
        total_links(0),
        num_forms(0),
        num_text_inputs(0),
        num_pswd_inputs(0),
        num_radio_inputs(0),
        num_check_inputs(0),
        action_other_domain(0),
        total_actions(0),
        img_other_domain(0),
        total_imgs(0),
        num_script_tags(0) {}

  ~PageFeatureState() {}
};

// Per-frame state
struct PhishingDOMFeatureExtractor::FrameData {
  // This is our reference to document.all, which is an iterator over all
  // of the elements in the document.  It keeps track of our current position.
  WebKit::WebNodeCollection elements;
  // The domain of the document URL, stored here so that we don't need to
  // recompute it every time it's needed.
  std::string domain;
};

PhishingDOMFeatureExtractor::PhishingDOMFeatureExtractor(
    RenderView* render_view)
    : render_view_(render_view),
      ALLOW_THIS_IN_INITIALIZER_LIST(method_factory_(this)) {
  Clear();
}

PhishingDOMFeatureExtractor::~PhishingDOMFeatureExtractor() {
  // The RenderView should have called CancelPendingExtraction() before
  // we are destroyed.
  CheckNoPendingExtraction();
}

void PhishingDOMFeatureExtractor::ExtractFeatures(
    FeatureMap* features,
    DoneCallback* done_callback) {
  // The RenderView should have called CancelPendingExtraction() before
  // starting a new extraction, so DCHECK this.
  CheckNoPendingExtraction();
  // However, in an opt build, we will go ahead and clean up the pending
  // extraction so that we can start in a known state.
  CancelPendingExtraction();

  features_ = features;
  done_callback_.reset(done_callback);
  MessageLoop::current()->PostTask(
      FROM_HERE,
      method_factory_.NewRunnableMethod(
          &PhishingDOMFeatureExtractor::ExtractFeaturesWithTimeout));
}

void PhishingDOMFeatureExtractor::CancelPendingExtraction() {
  // Cancel any pending callbacks, and clear our state.
  method_factory_.RevokeAll();
  Clear();
}

void PhishingDOMFeatureExtractor::ExtractFeaturesWithTimeout() {
  if (!cur_frame_) {
    WebKit::WebView* web_view = render_view_->webview();
    if (!web_view) {
      // When the WebView is going away, the render view should have called
      // CancelPendingExtraction() which should have stopped any pending work,
      // so this case should not happen.
      NOTREACHED();
      RunCallback(false);
      return;
    }
    cur_frame_ = web_view->mainFrame();
    page_feature_state_.reset(new PageFeatureState);
  }

  for (; cur_frame_;
       cur_frame_ = cur_frame_->traverseNext(false /* don't wrap around */)) {
    WebKit::WebNode cur_node;
    if (cur_frame_data_.get()) {
      // We're resuming traversal of a frame, so just advance to the next node.
      cur_node = cur_frame_data_->elements.nextItem();
    } else {
      // We just moved to a new frame, so update our frame state
      // and advance to the first element.
      if (!ResetFrameData()) {
        // Nothing in this frame, move on to the next one.
        LOG(WARNING) << "No content in frame, skipping";
        continue;
      }
      cur_node = cur_frame_data_->elements.firstItem();
    }

    for (; !cur_node.isNull();
         cur_node = cur_frame_data_->elements.nextItem()) {
      if (!cur_node.isElementNode()) {
        continue;
      }
      WebKit::WebElement element = cur_node.to<WebKit::WebElement>();
      if (element.hasTagName("a")) {
        HandleLink(element);
      } else if (element.hasTagName("form")) {
        HandleForm(element);
      } else if (element.hasTagName("img")) {
        HandleImage(element);
      } else if (element.hasTagName("input")) {
        HandleInput(element);
      } else if (element.hasTagName("script")) {
        HandleScript(element);
      }

      // TODO(bryner): stop if too much time has elapsed, and add histograms
      // for the time spent processing.
    }

    // We're done with this frame, recalculate the FrameData when we
    // advance to the next frame.
    cur_frame_data_.reset();
  }

  InsertFeatures();
  RunCallback(true);
}

void PhishingDOMFeatureExtractor::HandleLink(
    const WebKit::WebElement& element) {
  // Count the number of times we link to a different host.
  if (!element.hasAttribute("href")) {
    DLOG(INFO) << "Skipping anchor tag with no href";
    return;
  }

  // Retrieve the link and resolve the link in case it's relative.
  WebKit::WebURL full_url = element.document().completeURL(
      element.getAttribute("href"));

  std::string domain;
  bool is_external = IsExternalDomain(full_url, &domain);
  if (domain.empty()) {
    LOG(ERROR) << "Could not extract domain from link: " << full_url;
    return;
  }

  if (is_external) {
    ++page_feature_state_->external_links;

    // Record each unique domain that we link to.
    page_feature_state_->external_domains.insert(domain);
  }

  // Check how many are https links.
  if (GURL(full_url).SchemeIs("https")) {
    ++page_feature_state_->secure_links;
  }

  ++page_feature_state_->total_links;
}

void PhishingDOMFeatureExtractor::HandleForm(
    const WebKit::WebElement& element) {
  // Increment the number of forms on this page.
  ++page_feature_state_->num_forms;

  // Record whether the action points to a different domain.
  if (!element.hasAttribute("action")) {
    return;
  }

  WebKit::WebURL full_url = element.document().completeURL(
      element.getAttribute("action"));

  std::string domain;
  bool is_external = IsExternalDomain(full_url, &domain);
  if (domain.empty()) {
    LOG(ERROR) << "Could not extract domain from form action: " << full_url;
    return;
  }

  if (is_external) {
    ++page_feature_state_->action_other_domain;
  }
  ++page_feature_state_->total_actions;
}

void PhishingDOMFeatureExtractor::HandleImage(
    const WebKit::WebElement& element) {
  if (!element.hasAttribute("src")) {
    DLOG(INFO) << "Skipping img tag with no src";
  }

  // Record whether the image points to a different domain.
  WebKit::WebURL full_url = element.document().completeURL(
      element.getAttribute("src"));
  std::string domain;
  bool is_external = IsExternalDomain(full_url, &domain);
  if (domain.empty()) {
    LOG(ERROR) << "Could not extract domain from image src: " << full_url;
    return;
  }

  if (is_external) {
    ++page_feature_state_->img_other_domain;
  }
  ++page_feature_state_->total_imgs;
}

void PhishingDOMFeatureExtractor::HandleInput(
    const WebKit::WebElement& element) {
  // The HTML spec says that if the type is unspecified, it defaults to text.
  // In addition, any unrecognized type will be treated as a text input.
  //
  // Note that we use the attribute value rather than
  // WebFormControlElement::formControlType() for consistency with the
  // way the phishing classification model is created.
  std::string type = element.getAttribute("type").utf8();
  StringToLowerASCII(&type);
  if (type == "password") {
    ++page_feature_state_->num_pswd_inputs;
  } else if (type == "radio") {
    ++page_feature_state_->num_radio_inputs;
  } else if (type == "checkbox") {
    ++page_feature_state_->num_check_inputs;
  } else if (type != "submit" && type != "reset" && type != "file" &&
             type != "hidden" && type != "image" && type != "button") {
    // Note that there are a number of new input types in HTML5 that are not
    // handled above.  For now, we will consider these as text inputs since
    // they could be used to capture user input.
    ++page_feature_state_->num_text_inputs;
  }
}

void PhishingDOMFeatureExtractor::HandleScript(
    const WebKit::WebElement& element) {
  ++page_feature_state_->num_script_tags;
}

void PhishingDOMFeatureExtractor::CheckNoPendingExtraction() {
  DCHECK(!done_callback_.get());
  DCHECK(!cur_frame_data_.get());
  DCHECK(!cur_frame_);
  if (done_callback_.get() || cur_frame_data_.get() || cur_frame_) {
    LOG(ERROR) << "Extraction in progress, missing call to "
               << "CancelPendingExtraction";
  }
}

void PhishingDOMFeatureExtractor::RunCallback(bool success) {
  DCHECK(done_callback_.get());
  done_callback_->Run(success);
  Clear();
}

void PhishingDOMFeatureExtractor::Clear() {
  features_ = NULL;
  done_callback_.reset(NULL);
  cur_frame_data_.reset(NULL);
  cur_frame_ = NULL;
}

bool PhishingDOMFeatureExtractor::ResetFrameData() {
  DCHECK(cur_frame_);
  DCHECK(!cur_frame_data_.get());

  WebKit::WebDocument doc = cur_frame_->document();
  if (doc.isNull()) {
    return false;
  }
  cur_frame_data_.reset(new FrameData());
  cur_frame_data_->elements = doc.all();
  cur_frame_data_->domain =
      net::RegistryControlledDomainService::GetDomainAndRegistry(
          cur_frame_->url());
  return true;
}

bool PhishingDOMFeatureExtractor::IsExternalDomain(const GURL& url,
                                                   std::string* domain) const {
  DCHECK(domain);
  DCHECK(cur_frame_data_.get());

  if (cur_frame_data_->domain.empty()) {
    return false;
  }

  // TODO(bryner): Ensure that the url encoding is consistent with the features
  // in the model.
  if (url.HostIsIPAddress()) {
    domain->assign(url.host());
  } else {
    domain->assign(net::RegistryControlledDomainService::GetDomainAndRegistry(
        url));
  }

  return !domain->empty() && *domain != cur_frame_data_->domain;
}

void PhishingDOMFeatureExtractor::InsertFeatures() {
  DCHECK(page_feature_state_.get());
  features_->Clear();

  if (page_feature_state_->total_links > 0) {
    // Add a feature for the fraction of times the page links to an external
    // domain vs. an internal domain.
    double link_freq = static_cast<double>(
        page_feature_state_->external_links) /
        page_feature_state_->total_links;
    features_->AddRealFeature(features::kPageExternalLinksFreq, link_freq);

    // Add a feature for each unique domain that we're linking to
    for (base::hash_set<std::string>::iterator it =
             page_feature_state_->external_domains.begin();
         it != page_feature_state_->external_domains.end(); ++it) {
      features_->AddBooleanFeature(features::kPageLinkDomain + *it);
    }

    // Fraction of links that use https.
    double secure_freq = static_cast<double>(
        page_feature_state_->secure_links) / page_feature_state_->total_links;
    features_->AddRealFeature(features::kPageSecureLinksFreq, secure_freq);
  }

  // Record whether forms appear and whether various form elements appear.
  if (page_feature_state_->num_forms > 0) {
    features_->AddBooleanFeature(features::kPageHasForms);
  }
  if (page_feature_state_->num_text_inputs > 0) {
    features_->AddBooleanFeature(features::kPageHasTextInputs);
  }
  if (page_feature_state_->num_pswd_inputs > 0) {
    features_->AddBooleanFeature(features::kPageHasPswdInputs);
  }
  if (page_feature_state_->num_radio_inputs > 0) {
    features_->AddBooleanFeature(features::kPageHasRadioInputs);
  }
  if (page_feature_state_->num_check_inputs > 0) {
    features_->AddBooleanFeature(features::kPageHasCheckInputs);
  }

  // Record fraction of form actions that point to a different domain.
  if (page_feature_state_->total_actions > 0) {
    double action_freq = static_cast<double>(
        page_feature_state_->action_other_domain) /
        page_feature_state_->total_actions;
    features_->AddRealFeature(features::kPageActionOtherDomainFreq,
                              action_freq);
  }

  // Record how many image src attributes point to a different domain.
  if (page_feature_state_->total_imgs > 0) {
    double img_freq = static_cast<double>(
        page_feature_state_->img_other_domain) /
        page_feature_state_->total_imgs;
    features_->AddRealFeature(features::kPageImgOtherDomainFreq, img_freq);
  }

  // Record number of script tags (discretized for numerical stability.)
  if (page_feature_state_->num_script_tags > 1) {
    features_->AddBooleanFeature(features::kPageNumScriptTagsGTOne);
    if (page_feature_state_->num_script_tags > 6) {
      features_->AddBooleanFeature(features::kPageNumScriptTagsGTSix);
    }
  }
}

}  // namespace safe_browsing
