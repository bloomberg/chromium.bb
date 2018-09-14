// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPEECH_EXTENSION_API_TTS_ENGINE_DELEGATE_FACTORY_IMPL_H_
#define CHROME_BROWSER_SPEECH_EXTENSION_API_TTS_ENGINE_DELEGATE_FACTORY_IMPL_H_

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "chrome/browser/speech/extension_api/tts_engine_delegate_impl.h"
#include "chrome/browser/speech/tts_controller.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

// Factory that creates an instance of TtsEngineDelegateImpl given a
// BrowserContext. Implements the TtsEngineDelegateFactory interface to abstract
// away the details of chrome extensions from chrome/browser/speech.
class TtsEngineDelegateFactoryImpl : public BrowserContextKeyedServiceFactory,
                                     public TtsEngineDelegateFactory {
 public:
  // Returns the instance of TtsEngineDelegateImpl associated with
  // |browser_context| (creating one if none exists).
  static TtsEngineDelegateImpl* GetForBrowserContext(
      content::BrowserContext* browser_context);

  // Returns an instance of the factory singleton.
  static TtsEngineDelegateFactoryImpl* GetInstance();

  // Overridden from TtsEngineDelegateFactory:
  TtsEngineDelegate* GetTtsEngineDelegateForBrowserContext(
      content::BrowserContext* browser_context) override;

 private:
  friend struct base::DefaultSingletonTraits<TtsEngineDelegateFactoryImpl>;

  TtsEngineDelegateFactoryImpl();
  ~TtsEngineDelegateFactoryImpl() override;

  // Overridden from BrowserContextKeyedDelegateFactoryImpl:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;
  bool ServiceIsNULLWhileTesting() const override;
  bool ServiceIsCreatedWithBrowserContext() const override;

  DISALLOW_COPY_AND_ASSIGN(TtsEngineDelegateFactoryImpl);
};

#endif  // CHROME_BROWSER_SPEECH_EXTENSION_API_TTS_ENGINE_DELEGATE_FACTORY_IMPL_H_
