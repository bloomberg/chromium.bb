// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/renderer/form_tracker.h"

#include "components/autofill/content/renderer/form_autofill_util.h"
#include "content/public/renderer/document_state.h"
#include "content/public/renderer/navigation_state.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/WebKit/public/web/WebInputElement.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebUserGestureIndicator.h"
#include "third_party/WebKit/public/web/modules/password_manager/WebFormElementObserver.h"
#include "third_party/WebKit/public/web/modules/password_manager/WebFormElementObserverCallback.h"
#include "ui/base/page_transition_types.h"

using blink::WebDocumentLoader;
using blink::WebInputElement;
using blink::WebFormControlElement;
using blink::WebFormElement;

namespace autofill {

class FormTracker::FormElementObserverCallback
    : public blink::WebFormElementObserverCallback {
 public:
  explicit FormElementObserverCallback(FormTracker* tracker)
      : tracker_(tracker) {}
  ~FormElementObserverCallback() override = default;

  void ElementWasHiddenOrRemoved() override {
    tracker_->FireInferredFormSubmission(
        SubmissionSource::DOM_MUTATION_AFTER_XHR);
  }

 private:
  FormTracker* tracker_;

  DISALLOW_COPY_AND_ASSIGN(FormElementObserverCallback);
};

FormTracker::FormTracker(content::RenderFrame* render_frame)
    : content::RenderFrameObserver(render_frame), weak_ptr_factory_(this) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(form_tracker_sequence_checker_);
}

FormTracker::~FormTracker() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(form_tracker_sequence_checker_);
  ResetLastInteractedElements();
}

void FormTracker::AddObserver(Observer* observer) {
  DCHECK(observer);
  DCHECK_CALLED_ON_VALID_SEQUENCE(form_tracker_sequence_checker_);
  observers_.AddObserver(observer);
}

void FormTracker::RemoveObserver(Observer* observer) {
  DCHECK(observer);
  DCHECK_CALLED_ON_VALID_SEQUENCE(form_tracker_sequence_checker_);
  observers_.RemoveObserver(observer);
}

void FormTracker::AjaxSucceeded() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(form_tracker_sequence_checker_);
  FireSubmissionIfFormDisappear(SubmissionSource::XHR_SUCCEEDED);
}

void FormTracker::TextFieldDidChange(const WebFormControlElement& element) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(form_tracker_sequence_checker_);
  DCHECK(ToWebInputElement(&element) || form_util::IsTextAreaElement(element));

  if (ignore_text_changes_)
    return;

  // Disregard text changes that aren't caused by user gestures or pastes. Note
  // that pastes aren't necessarily user gestures because Blink's conception of
  // user gestures is centered around creating new windows/tabs.
  if (user_gesture_required_ &&
      !blink::WebUserGestureIndicator::IsProcessingUserGesture() &&
      !render_frame()->IsPasting())
    return;

  // We post a task for doing the Autofill as the caret position is not set
  // properly at this point (http://bugs.webkit.org/show_bug.cgi?id=16976) and
  // it is needed to trigger autofill.
  weak_ptr_factory_.InvalidateWeakPtrs();
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&FormTracker::TextFieldDidChangeImpl,
                            weak_ptr_factory_.GetWeakPtr(), element));
}

void FormTracker::FireProbablyFormSubmittedForTesting() {
  FireProbablyFormSubmitted();
}

void FormTracker::TextFieldDidChangeImpl(const WebFormControlElement& element) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(form_tracker_sequence_checker_);
  // If the element isn't focused then the changes don't matter. This check is
  // required to properly handle IME interactions.
  if (!element.Focused())
    return;

  const WebInputElement* input_element = ToWebInputElement(&element);
  if (!input_element)
    return;

  if (element.Form().IsNull()) {
    last_interacted_formless_element_ = *input_element;
  } else {
    last_interacted_form_ = element.Form();
  }

  for (auto& observer : observers_) {
    observer.OnProvisionallySaveForm(
        element.Form(), *input_element,
        Observer::ElementChangeSource::TEXTFIELD_CHANGED);
  }
}

void FormTracker::DidCommitProvisionalLoad(bool is_new_navigation,
                                           bool is_same_document_navigation) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(form_tracker_sequence_checker_);
  if (!is_same_document_navigation)
    return;

  FireSubmissionIfFormDisappear(SubmissionSource::SAME_DOCUMENT_NAVIGATION);
}

void FormTracker::DidStartProvisionalLoad(WebDocumentLoader* document_loader) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(form_tracker_sequence_checker_);
  blink::WebLocalFrame* navigated_frame = render_frame()->GetWebFrame();
  // Ony handle main frame.
  if (navigated_frame->Parent())
    return;

  // Bug fix for crbug.com/368690. isProcessingUserGesture() is false when
  // the user is performing actions outside the page (e.g. typed url,
  // history navigation). We don't want to trigger saving in these cases.
  content::DocumentState* document_state =
      content::DocumentState::FromDocumentLoader(document_loader);
  DCHECK(document_state);
  if (!document_state)
    return;

  content::NavigationState* navigation_state =
      document_state->navigation_state();

  DCHECK(navigation_state);
  if (!navigation_state)
    return;

  ui::PageTransition type = navigation_state->GetTransitionType();
  if (ui::PageTransitionIsWebTriggerable(type) &&
      ui::PageTransitionIsNewNavigation(type) &&
      !blink::WebUserGestureIndicator::IsProcessingUserGesture(
          navigated_frame)) {
    FireProbablyFormSubmitted();
  }
}

void FormTracker::FrameDetached() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(form_tracker_sequence_checker_);
  FireInferredFormSubmission(SubmissionSource::FRAME_DETACHED);
}

void FormTracker::WillSendSubmitEvent(const WebFormElement& form) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(form_tracker_sequence_checker_);
  last_interacted_form_ = form;
  for (auto& observer : observers_) {
    observer.OnProvisionallySaveForm(
        form, blink::WebInputElement(),
        Observer::ElementChangeSource::WILL_SEND_SUBMIT_EVENT);
  }
}

void FormTracker::WillSubmitForm(const WebFormElement& form) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(form_tracker_sequence_checker_);
  FireFormSubmitted(form);
}

void FormTracker::OnDestruct() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(form_tracker_sequence_checker_);
  ResetLastInteractedElements();
}

void FormTracker::FireFormSubmitted(const blink::WebFormElement& form) {
  for (auto& observer : observers_)
    observer.OnFormSubmitted(form);
  ResetLastInteractedElements();
}

void FormTracker::FireProbablyFormSubmitted() {
  for (auto& observer : observers_)
    observer.OnProbablyFormSubmitted();
  ResetLastInteractedElements();
}

void FormTracker::FireInferredFormSubmission(SubmissionSource source) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(form_tracker_sequence_checker_);
  for (auto& observer : observers_)
    observer.OnInferredFormSubmission(source);
  ResetLastInteractedElements();
}

void FormTracker::FireSubmissionIfFormDisappear(SubmissionSource source) {
  if (CanInferFormSubmitted()) {
    FireInferredFormSubmission(source);
    return;
  }
  TrackElement();
}

bool FormTracker::CanInferFormSubmitted() {
  // If last interacted form is available, assume form submission only if the
  // form is now gone, either invisible or removed from the DOM.
  // Otherwise (i.e., there is no form tag), we check if the last element the
  // user has interacted with are gone, to decide if submission has occurred.
  if (!last_interacted_form_.IsNull())
    return !form_util::AreFormContentsVisible(last_interacted_form_);
  else if (!last_interacted_formless_element_.IsNull())
    return !form_util::IsWebElementVisible(last_interacted_formless_element_);

  return false;
}

void FormTracker::TrackElement() {
  // Already has observer for last interacted element.
  if (form_element_observer_)
    return;
  std::unique_ptr<FormElementObserverCallback> callback =
      std::make_unique<FormElementObserverCallback>(this);

  if (!last_interacted_form_.IsNull()) {
    form_element_observer_ = blink::WebFormElementObserver::Create(
        last_interacted_form_, std::move(callback));
  } else if (!last_interacted_formless_element_.IsNull()) {
    form_element_observer_ = blink::WebFormElementObserver::Create(
        last_interacted_formless_element_, std::move(callback));
  }
}

void FormTracker::ResetLastInteractedElements() {
  last_interacted_form_.Reset();
  last_interacted_formless_element_.Reset();
  if (form_element_observer_) {
    form_element_observer_->Disconnect();
    form_element_observer_ = nullptr;
  }
}

}  // namespace autofill
