#!/usr/bin/python
# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import glob
import logging
import os

import pyauto_functional  # Must be imported before pyauto
import pyauto


class TranslateTest(pyauto.PyUITest):
  """Tests that translate works correctly"""

  spanish = 'es'
  after_translate = 'AFTER_TRANSLATE'
  before_translate = 'BEFORE_TRANSLATE'
  translating = 'TRANSLATING'
  translation_error = 'TRANSLATION_ERROR'

  def Debug(self):
    """ Test method for experimentation. """
    import pprint
    pp = pprint.PrettyPrinter(indent=2)
    while True:
      raw_input('Hit <enter> to dump translate info.. ')
      pp.pprint(self.GetTranslateInfo())

  def _GetURLForDataDirFile(self, filename):
    """Return the file URL for the given file in the data directory."""
    return self.GetFileURLForPath(os.path.join(self.DataDir(), filename))

  def _GetDefaultSpanishURL(self):
    return self._GetURLForDataDirFile(
        os.path.join('translate', self.spanish, 'google.html'))

  def _GetDefaultEnglishURL(self):
    return self._GetURLForDataDirFile('title1.html')

  def _NavigateAndWaitForBar(self, url):
    self.NavigateToURL(url)
    self.WaitForInfobarCount(1)

  def _ClickTranslateUntilSuccess(self, window_index=0, tab_index=0):
    """Since the translate can fail due to server error, continue trying until
       it is successful or until it has tried too many times."""
    max_tries = 10
    curr_try = 0
    while (curr_try < max_tries and
           not self.ClickTranslateBarTranslate(window_index=window_index,
                                               tab_index=tab_index)):
      curr_try = curr_try + 1
    if curr_try == max_tries:
      self.fail('Translation failed more than %d times.' % max_tries)

  def _AssertTranslateWorks(self, url, original_language):
    """Translate the page at the given URL and assert that the page has been
       translated.
    """
    self._NavigateAndWaitForBar(url)
    translate_info = self.GetTranslateInfo()
    self.assertEqual(original_language, translate_info['original_language'])
    self.assertFalse(translate_info['page_translated'])
    self.assertTrue(translate_info['can_translate_page'])
    self.assertTrue('translate_bar' in translate_info)
    self._ClickTranslateUntilSuccess()
    translate_info = self.GetTranslateInfo()
    self.assertTrue(translate_info['can_translate_page'])
    self.assertTrue('translate_bar' in translate_info)
    translate_bar = translate_info['translate_bar']
    self.assertEquals(self.after_translate,
                      translate_info['translate_bar']['bar_state'])
    self.assertEquals(original_language,
                      translate_bar['original_lang_code'])

  def testTranslate(self):
    """Tests that a page was translated if the user chooses to translate."""
    self._AssertTranslateWorks(self._GetDefaultSpanishURL(), self.spanish)

  def testNoTranslate(self):
    """Tests that a page isn't translated if the user declines translate."""
    self._NavigateAndWaitForBar(self._GetDefaultSpanishURL())
    self.PerformActionOnInfobar('dismiss', 0)
    translate_info = self.GetTranslateInfo()
    self.assertEqual(self.spanish, translate_info['original_language'])
    self.assertFalse(translate_info['page_translated'])
    self.assertTrue(translate_info['can_translate_page'])
    # If the user goes to the site again, the infobar should drop down but
    # the page should not be automatically translated.
    self._NavigateAndWaitForBar(self._GetDefaultSpanishURL())
    translate_info = self.GetTranslateInfo()
    self.assertFalse(translate_info['page_translated'])
    self.assertTrue(translate_info['can_translate_page'])
    self.assertTrue('translate_bar' in translate_info)
    self.assertEquals(self.before_translate,
                      translate_info['translate_bar']['bar_state'])

  def testNeverTranslateLanguage(self):
    """Tests that blacklisting a language for translate works."""
    self._NavigateAndWaitForBar(self._GetDefaultSpanishURL())
    self.SelectTranslateOption('never_translate_language')
    translate_info = self.GetTranslateInfo()
    self.assertEqual(self.spanish, translate_info['original_language'])
    self.assertFalse(translate_info['page_translated'])
    self.assertFalse(translate_info['can_translate_page'])
    spanish_wikipedia = 'http://es.wikipedia.org/wiki/Wikipedia:Portada'
    self.NavigateToURL(spanish_wikipedia)
    translate_info = self.GetTranslateInfo()
    self.assertEqual(self.spanish, translate_info['original_language'])
    self.assertFalse(translate_info['page_translated'])
    self.assertFalse(translate_info['can_translate_page'])

  def testAlwaysTranslateLanguage(self):
    """Tests that the always translate a language option works."""
    self._NavigateAndWaitForBar(self._GetDefaultSpanishURL())
    self.SelectTranslateOption('toggle_always_translate')
    self._ClickTranslateUntilSuccess()
    translate_info = self.GetTranslateInfo()
    self.assertEquals(self.spanish, translate_info['original_language'])
    self.assertTrue(translate_info['page_translated'])
    self.assertTrue(translate_info['can_translate_page'])
    self.assertTrue('translate_bar' in translate_info)
    self.assertEquals(self.after_translate,
                      translate_info['translate_bar']['bar_state'])
    # Go to another spanish site and verify that it is translated.
    # Note that the 'This page has been translated..." bar will show up.
    self._NavigateAndWaitForBar(
        'http://es.wikipedia.org/wiki/Wikipedia:Portada')
    translate_info = self.GetTranslateInfo()
    self.assertTrue('translate_bar' in translate_info)
    curr_bar_state = translate_info['translate_bar']['bar_state']
    # We don't care whether the translation has finished, just that it is
    # trying to translate.
    self.assertTrue(curr_bar_state is self.after_translate or
                    self.translating or self.translation_error,
                    'Bar state was %s.' % curr_bar_state)

  def testNeverTranslateSite(self):
    """Tests that blacklisting a site for translate works."""
    spanish_google = 'http://www.google.com/webhp?hl=es'
    self._NavigateAndWaitForBar(spanish_google)
    self.SelectTranslateOption('never_translate_site')
    translate_info = self.GetTranslateInfo()
    self.assertFalse(translate_info['page_translated'])
    self.assertFalse(translate_info['can_translate_page'])
    french_google = 'http://www.google.com/webhp?hl=fr'
    # Go to another page in the same site and verify that the page can't be
    # translated even though it's in a different language.
    self.NavigateToURL(french_google)
    translate_info = self.GetTranslateInfo()
    self.assertFalse(translate_info['page_translated'])
    self.assertFalse(translate_info['can_translate_page'])

  def testRevert(self):
    """Tests that reverting a site to its original language works."""
    self._NavigateAndWaitForBar(self._GetDefaultSpanishURL())
    self._ClickTranslateUntilSuccess()
    self.RevertPageTranslation()
    translate_info = self.GetTranslateInfo()
    self.assertFalse(translate_info['page_translated'])
    self.assertTrue(translate_info['can_translate_page'])

  def testBarNotVisibleOnSSLErrorPage(self):
    """Test Translate bar is not visible on SSL error pages."""
    self._NavigateAndWaitForBar(self._GetDefaultSpanishURL())
    translate_info = self.GetTranslateInfo()
    self.assertTrue('translate_bar' in translate_info)
    self.assertTrue(translate_info['can_translate_page'])
    # This page is an ssl error page.
    self.NavigateToURL('https://www.sourceforge.net')
    self.WaitForInfobarCount(0)
    translate_info = self.GetTranslateInfo()
    self.assertFalse('translate_bar' in translate_info)

  def testBarNotVisibleOnEnglishPage(self):
    """Test Translate bar is not visible on English pages."""
    self._NavigateAndWaitForBar(self._GetDefaultSpanishURL())
    translate_info = self.GetTranslateInfo()
    self.assertTrue('translate_bar' in translate_info)
    self.assertTrue(translate_info['can_translate_page'])
    # With the translate bar visible in same tab open an English page.
    self.NavigateToURL(self._GetDefaultEnglishURL())
    self.WaitForInfobarCount(0)
    translate_info = self.GetTranslateInfo()
    self.assertFalse('translate_bar' in translate_info)

  def testTranslateDiffURLs(self):
    """Test that HTTP, HTTPS, and file urls all get translated."""
    http_url = 'http://www.google.com/webhp?hl=es'
    https_url = 'https://www.google.com/webhp?hl=es'
    file_url = self._GetDefaultSpanishURL()
    for url in (http_url, https_url, file_url):
      self._AssertTranslateWorks(url, self.spanish)

  def testNeverTranslateLanguage(self):
    """Verify no translate bar for blacklisted language."""
    self._NavigateAndWaitForBar(self._GetDefaultSpanishURL())
    self.SelectTranslateOption('never_translate_language')
    self.assertFalse(self.GetBrowserInfo()['windows'][0]['tabs'][0]['infobars'])
    # Navigate to a page that will show the translate bar, then navigate away.
    self._NavigateAndWaitForBar(
        self._GetURLForDataDirFile('french_page.html'))
    self.NavigateToURL('http://es.wikipedia.org/wiki/Wikipedia:Portada')
    self.WaitForInfobarCount(0)
    translate_info = self.GetTranslateInfo()
    self.assertFalse('translate_bar' in translate_info)
    self.assertFalse(translate_info['page_translated'])
    self.assertFalse(translate_info['can_translate_page'])

  def testNotranslateMetaTag(self):
    """Test "notranslate" meta tag."""
    self._NavigateAndWaitForBar(self._GetDefaultSpanishURL())
    self.NavigateToURL(self._GetURLForDataDirFile(
          os.path.join('translate', 'notranslate_meta_tag.html')))
    self.WaitForInfobarCount(0)
    translate_info = self.GetTranslateInfo()
    self.assertFalse('translate_bar' in translate_info)

  def testToggleTranslateOption(self):
    """Test to toggle the 'Always Translate' option."""
    self._NavigateAndWaitForBar(self._GetDefaultSpanishURL())
    # Assert the default.
    translate_info = self.GetTranslateInfo()
    self.assertFalse(translate_info['page_translated'])
    self.assertTrue('translate_bar' in translate_info)
    # Select 'Always Translate'.
    self.SelectTranslateOption('toggle_always_translate')
    self._ClickTranslateUntilSuccess()
    translate_info = self.GetTranslateInfo()
    self.assertTrue(translate_info['page_translated'])
    # Reload the tab and confirm the page was translated.
    self.GetBrowserWindow(0).GetTab(0).Reload()
    self.WaitForInfobarCount(1)
    success = self.WaitUntilTranslateComplete()
    # Sometimes the translation fails. Continue clicking until it succeeds.
    if not success:
      self._ClickTranslateUntilSuccess()
    # Uncheck 'Always Translate'
    self.SelectTranslateOption('toggle_always_translate')
    self.PerformActionOnInfobar('dismiss', 0)
    translate_info = self.GetTranslateInfo()
    self.assertTrue(translate_info['page_translated'])
    self.assertFalse('translate_bar' in translate_info)
    # Reload the tab and confirm that the page has not been translated.
    self.GetBrowserWindow(0).GetTab(0).Reload()
    translate_info = self.GetTranslateInfo()
    self.assertFalse(translate_info['page_translated'])
    self.assertTrue('translate_bar' in translate_info)

  def testSessionRestore(self):
    """Test that session restore restores the translate infobar and other
       translate settings.
    """
    # Due to crbug.com/51439, we must open two tabs here.
    self.NavigateToURL("http://www.news.google.com")
    self.AppendTab(pyauto.GURL("http://www.google.com/webhp?hl=es"))
    self.WaitForInfobarCount(1, tab_index=1)
    translate_info = self.GetTranslateInfo(tab_index=1)
    self.assertTrue('translate_bar' in translate_info)
    self.SelectTranslateOption('toggle_always_translate', tab_index=1)
    self._ClickTranslateUntilSuccess(tab_index=1)
    self.SetPrefs(pyauto.kRestoreOnStartup, 1)
    self.RestartBrowser(clear_profile=False)
    self.WaitForInfobarCount(1, tab_index=1)
    self.WaitUntilTranslateComplete()
    translate_info = self.GetTranslateInfo(tab_index=1)
    self.assertTrue('translate_bar' in translate_info)
    # Sometimes translation fails. We don't really care whether it succeededs,
    # just that a translation was attempted.
    self.assertNotEqual(self.before_translate,
                        translate_info['translate_bar']['bar_state'])

  def testGoBackAndForwardToTranslatePage(self):
    """Tests that translate bar re-appears after backward and forward
       navigation to a page that can be translated.
    """
    no_trans_url = self._GetDefaultEnglishURL()
    trans_url = self._GetDefaultSpanishURL()
    self._NavigateAndWaitForBar(trans_url)
    translate_info = self.GetTranslateInfo()
    self.assertTrue('translate_bar' in translate_info)
    self.NavigateToURL(no_trans_url)
    self.WaitForInfobarCount(0)
    self.assertFalse('translate_bar' in self.GetTranslateInfo())
    # Go back to the page that should be translated and assert that the
    # translate bar re-appears.
    self.GetBrowserWindow(0).GetTab(0).GoBack()
    self.WaitForInfobarCount(1)
    self.assertTrue('translate_bar' in self.GetTranslateInfo())

    # Now test going forward.
    self.NavigateToURL(no_trans_url)
    translate_info = self.GetTranslateInfo()
    self.assertFalse('translate_bar' in translate_info)
    self._AssertTranslateWorks(trans_url, self.spanish)
    self.GetBrowserWindow(0).GetTab(0).GoBack()
    self.WaitForInfobarCount(0)
    translate_info = self.GetTranslateInfo()
    self.assertFalse('translate_bar' in translate_info)
    self.GetBrowserWindow(0).GetTab(0).GoForward()
    self.WaitForInfobarCount(1)
    translate_info = self.GetTranslateInfo()
    self.assertTrue(translate_info['can_translate_page'])
    self.assertTrue('translate_bar' in translate_info)

  def testForCrashedTab(self):
    """Tests that translate bar is dimissed if the renderer crashes."""
    crash_url = 'about:crash'
    self._NavigateAndWaitForBar(self._GetDefaultSpanishURL())
    translate_info = self.GetTranslateInfo()
    self.assertTrue('translate_bar' in translate_info)
    self.NavigateToURL(crash_url)
    self.WaitForInfobarCount(0)
    self.assertFalse('translate_bar' in self.GetTranslateInfo())

  def testTranslatePrefs(self):
    """Test the Prefs:Translate option."""
    # Assert defaults first.
    self.assertTrue(self.GetPrefsInfo().Prefs(pyauto.kEnableTranslate))
    # Uncheck.
    self.SetPrefs(pyauto.kEnableTranslate, False)
    self.NavigateToURL(self._GetDefaultSpanishURL())
    self.assertFalse('translate_bar' in self.GetTranslateInfo())
    # Restart the browser and assert the translate preference stays.
    self.RestartBrowser(clear_profile=False)
    self.assertFalse(self.GetPrefsInfo().Prefs(pyauto.kEnableTranslate))
    self.NavigateToURL(self._GetDefaultSpanishURL())
    self.assertFalse('translate_bar' in self.GetTranslateInfo())

  def testSeveralLanguages(self):
    """Verify translation for several languages.

    This assumes that there is a directory of directories in the data dir.
    The directory is called 'translate', and within that directory there are
    subdirectories for each language code. Ex. a subdirectory 'es' with Spanish
    html files.
    """
    corpora_path = os.path.join(self.DataDir(), 'translate')
    corp_dir = glob.glob(os.path.join(corpora_path, '??')) + \
               glob.glob(os.path.join(corpora_path, '??-??'))

    for language in corp_dir:
      files = glob.glob(os.path.join(language, '*.html*'))
      lang_code = os.path.basename(language)
      logging.debug('Starting language %s' % lang_code)
      # Translate each html file in the language directory.
      for lang_file in files:
        logging.debug('Starting file %s' % lang_file)
        lang_file = self.GetFileURLForPath(lang_file)
        self._AssertTranslateWorks(lang_file, lang_code)


if __name__ == '__main__':
  pyauto_functional.Main()
