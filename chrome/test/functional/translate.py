#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import glob
import logging
import os
import shutil
import tempfile

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
    while True:
      raw_input('Hit <enter> to dump translate info.. ')
      self.pprint(self.GetTranslateInfo())

  def _GetDefaultSpanishURL(self):
    return self.GetHttpURLForDataPath('translate', self.spanish, 'google.html')

  def _GetDefaultEnglishURL(self):
    return self.GetHttpURLForDataPath('title1.html')

  def _NavigateAndWaitForBar(self, url, window_index=0, tab_index=0):
    self.NavigateToURL(url, window_index, tab_index)
    self.assertTrue(self.WaitForInfobarCount(
        1, windex=window_index, tab_index=tab_index))

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
    self.SelectTranslateOption('decline_translation')
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
    self.assertTrue(self.WaitForInfobarCount(0))
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
    self.assertTrue(self.WaitForInfobarCount(0))
    translate_info = self.GetTranslateInfo()
    self.assertFalse('translate_bar' in translate_info)

  def testTranslateDiffURLs(self):
    """Test that HTTP, HTTPS, and file urls all get translated."""
    http_url = 'http://www.google.com/webhp?hl=es'
    https_url = 'https://www.google.com/webhp?hl=es'
    file_url = self._GetDefaultSpanishURL()
    for url in (http_url, https_url, file_url):
      self._AssertTranslateWorks(url, self.spanish)

  def testNotranslateMetaTag(self):
    """Test "notranslate" meta tag."""
    self._NavigateAndWaitForBar(self._GetDefaultSpanishURL())
    self.NavigateToURL(self.GetFileURLForDataPath(
        'translate', 'notranslate_meta_tag.html'))
    self.assertTrue(self.WaitForInfobarCount(0))
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
    self.assertTrue(self.WaitForInfobarCount(1))
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
    self.assertTrue(self.WaitForInfobarCount(1, tab_index=1))
    translate_info = self.GetTranslateInfo(tab_index=1)
    self.assertTrue('translate_bar' in translate_info)
    self.SelectTranslateOption('toggle_always_translate', tab_index=1)
    self._ClickTranslateUntilSuccess(tab_index=1)
    self.SetPrefs(pyauto.kRestoreOnStartup, 1)
    self.RestartBrowser(clear_profile=False)
    self.assertTrue(self.WaitForInfobarCount(1, tab_index=1))
    self.WaitUntilTranslateComplete(tab_index=1)
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
    self.assertTrue(self.WaitForInfobarCount(0))
    self.assertFalse('translate_bar' in self.GetTranslateInfo())
    # Go back to the page that should be translated and assert that the
    # translate bar re-appears.
    self.GetBrowserWindow(0).GetTab(0).GoBack()
    self.assertTrue(self.WaitForInfobarCount(1))
    self.assertTrue('translate_bar' in self.GetTranslateInfo())

    # Now test going forward.
    self.NavigateToURL(no_trans_url)
    translate_info = self.GetTranslateInfo()
    self.assertFalse('translate_bar' in translate_info)
    self._AssertTranslateWorks(trans_url, self.spanish)
    self.GetBrowserWindow(0).GetTab(0).GoBack()
    self.assertTrue(self.WaitForInfobarCount(0))
    translate_info = self.GetTranslateInfo()
    self.assertFalse('translate_bar' in translate_info)
    self.GetBrowserWindow(0).GetTab(0).GoForward()
    self.assertTrue(self.WaitForInfobarCount(1))
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
    self.assertTrue(self.WaitForInfobarCount(0))
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

  def testAlwaysTranslateLanguageButton(self):
    """Test the always translate language button."""
    spanish_url = self._GetDefaultSpanishURL()
    self._NavigateAndWaitForBar(spanish_url)

    # The 'Always Translate' button doesn't show up until the user has clicked
    # 'Translate' for a language 3 times.
    for unused in range(3):
      self._ClickTranslateUntilSuccess()
      self.GetBrowserWindow(0).GetTab(0).Reload()

    # Click the 'Always Translate' button.
    self.assertTrue(self.GetTranslateInfo()\
        ['translate_bar']['always_translate_lang_button_showing'])
    self.SelectTranslateOption('click_always_translate_lang_button')
    # Navigate to another Spanish page and verify it was translated.
    self._NavigateAndWaitForBar('http://www.google.com/webhp?hl=es')
    self.WaitUntilTranslateComplete()
    # Assert that a translation was attempted. We don't care if it was error
    # or success.
    self.assertNotEqual(self.before_translate,
                        self.GetTranslateInfo()['translate_bar']['bar_state'])

  def testNeverTranslateLanguageButton(self):
    """Test the never translate language button."""
    spanish_url = self._GetDefaultSpanishURL()
    self._NavigateAndWaitForBar(spanish_url)

    # The 'Never Translate' button doesn't show up until the user has clicked
    # 'Nope' for a language 3 times.
    for unused in range(3):
      self.SelectTranslateOption('decline_translation')
      self.GetBrowserWindow(0).GetTab(0).Reload()

    # Click the 'Never Translate' button.
    self.assertTrue(self.GetTranslateInfo()\
        ['translate_bar']['never_translate_lang_button_showing'])
    self.SelectTranslateOption('click_never_translate_lang_button')
    # Navigate to another Spanish page and verify the page can't be translated.
    # First navigate to a French page, wait for bar, navigate to Spanish page
    # and wait for bar to go away.
    self._NavigateAndWaitForBar('http://www.google.com/webhp?hl=fr')
    self.NavigateToURL('http://www.google.com/webhp?hl=es')
    self.assertTrue(self.WaitForInfobarCount(0))
    self.assertFalse(self.GetTranslateInfo()['can_translate_page'])

  def testChangeTargetLanguageAlwaysTranslate(self):
    """Tests that the change target language option works with always translate
       on reload.

    This test is motivated by crbug.com/37313.
    """
    self._NavigateAndWaitForBar(self._GetDefaultSpanishURL())
    self._ClickTranslateUntilSuccess()
    # Select a different target language on translate info bar (French).
    self.ChangeTranslateToLanguage('French')
    translate_info = self.GetTranslateInfo()
    self.assertTrue('translate_bar' in translate_info)
    self.assertEqual('fr', translate_info['translate_bar']['target_lang_code'])
    # Select always translate Spanish to French.
    self.SelectTranslateOption('toggle_always_translate')
    # Reload the page and assert that the page has been translated to French.
    self.GetBrowserWindow(0).GetTab(0).Reload()
    self.WaitUntilTranslateComplete()
    translate_info = self.GetTranslateInfo()
    self.assertTrue(translate_info['page_translated'])
    self.assertTrue(translate_info['can_translate_page'])
    self.assertTrue('translate_bar' in translate_info)
    self.assertEqual('fr', translate_info['translate_bar']['target_lang_code'])

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

  def _CreateCrazyDownloads(self):
    """Doownload files with filenames containing special chars.
       The files are created on the fly and cleaned after use.
    """
    download_dir = self.GetDownloadDirectory().value()
    filename = os.path.join(self.DataDir(), 'downloads', 'crazy_filenames.txt')
    crazy_filenames = self.EvalDataFrom(filename)
    logging.info('Testing with %d crazy filenames' % len(crazy_filenames))

    def _CreateFile(name):
      """Create and fill the given file with some junk."""
      fp = open(name, 'w')  # name could be unicode
      print >>fp, 'This is a junk file named %s. ' % repr(name) * 100
      fp.close()

    # Temp dir for hosting crazy filenames.
    temp_dir = tempfile.mkdtemp(prefix='download')
    # Filesystem-interfacing functions like os.listdir() need to
    # be given unicode strings to "do the right thing" on win.
    # Ref: http://boodebr.org/main/python/all-about-python-and-unicode
    try:
      for filename in crazy_filenames:  # filename is unicode.
        utf8_filename = filename.encode('utf-8')
        file_path = os.path.join(temp_dir, utf8_filename)
        _CreateFile(os.path.join(temp_dir, filename))  # unicode file.
        file_url = self.GetFileURLForPath(file_path)
        downloaded_file = os.path.join(download_dir, filename)
        os.path.exists(downloaded_file) and os.remove(downloaded_file)
        pre_download_ids = [x['id']
                            for x in self.GetDownloadsInfo().Downloads()]
        self.DownloadAndWaitForStart(file_url)
        # Wait for files and remove them as we go.
        self.WaitForAllDownloadsToComplete(pre_download_ids)
        os.path.exists(downloaded_file) and os.remove(downloaded_file)
    finally:
      shutil.rmtree(unicode(temp_dir))  # unicode so that win treats nicely.

  def testHistoryNotTranslated(self):
    """Tests navigating to History page with other languages."""
    # Build the history with non-English content.
    history_file = os.path.join(
        self.DataDir(), 'translate', 'crazy_history.txt')
    history_items = self.EvalDataFrom(history_file)
    for history_item in history_items:
       self.AddHistoryItem(history_item)
    # Navigate to a page that triggers the translate bar so we can verify that
    # it disappears on the history page.
    self._NavigateAndWaitForBar(self._GetDefaultSpanishURL())
    self.NavigateToURL('chrome://history/')
    self.assertTrue(self.WaitForInfobarCount(0))
    self.assertFalse('translate_bar' in self.GetTranslateInfo())

  def testDownloadsNotTranslated(self):
    """Tests navigating to the Downloads page with other languages."""
    # Build the download history with non-English content.
    self._CreateCrazyDownloads()
    # Navigate to a page that triggers the translate bar so we can verify that
    # it disappears on the downloads page.
    self._NavigateAndWaitForBar(self._GetDefaultSpanishURL())
    self.NavigateToURL('chrome://downloads/')
    self.assertTrue(self.WaitForInfobarCount(0))
    self.assertFalse('translate_bar' in self.GetTranslateInfo())

  def testAlwaysTranslateInIncognito(self):
    """Verify that pages aren't auto-translated in incognito windows."""
    url = self._GetDefaultSpanishURL()
    self._NavigateAndWaitForBar(url)
    info_before_translate = self.GetTranslateInfo()
    self.assertTrue('translate_bar' in info_before_translate)
    self.SelectTranslateOption('toggle_always_translate')

    # Navigate to a page in incognito and verify that it is not auto-translated.
    self.RunCommand(pyauto.IDC_NEW_INCOGNITO_WINDOW)
    self._NavigateAndWaitForBar(url, window_index=1)
    info_before_translate = self.GetTranslateInfo()
    self.assertTrue('translate_bar' in info_before_translate)
    self.assertFalse(info_before_translate['page_translated'])
    self.assertTrue(info_before_translate['can_translate_page'])
    self.assertEqual(self.before_translate,
                     info_before_translate['translate_bar']['bar_state'])

  def testMultipleTabsAndWindows(self):
    """Verify that translate infobar shows up in multiple tabs and windows."""
    url = self._GetDefaultSpanishURL()
    def _AssertTranslateInfobarShowing(window_index=0, tab_index=0):
      """Navigate to a Spanish page in the given window/tab and verify that the
         translate bar shows up.
      """
      self._NavigateAndWaitForBar(
          url, window_index=window_index, tab_index=tab_index)
      info_before_translate = self.GetTranslateInfo(window_index=window_index,
                                                    tab_index=tab_index)
      self.assertTrue('translate_bar' in info_before_translate)

    _AssertTranslateInfobarShowing()
    self.AppendTab(pyauto.GURL('about:blank'))
    _AssertTranslateInfobarShowing(tab_index=1)
    self.OpenNewBrowserWindow(True)
    _AssertTranslateInfobarShowing(window_index=1)
    self.AppendTab(pyauto.GURL('about:blank'), 1)
    _AssertTranslateInfobarShowing(window_index=1, tab_index=1)
    self.RunCommand(pyauto.IDC_NEW_INCOGNITO_WINDOW)
    _AssertTranslateInfobarShowing(window_index=2)
    self.AppendTab(pyauto.GURL('about:blank'), 2)
    _AssertTranslateInfobarShowing(window_index=2, tab_index=1)

  def testNoTranslateInfobarAfterNeverTranslate(self):
    """Verify Translate Info bar should not stay on the page after opting
       Never translate the page"""
    url = self._GetDefaultSpanishURL()
    self._NavigateAndWaitForBar(url)
    self.SelectTranslateOption('never_translate_language')
    self.assertFalse(self.GetBrowserInfo()['windows'][0]['tabs'][0]['infobars'])

if __name__ == '__main__':
  pyauto_functional.Main()
