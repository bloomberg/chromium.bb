# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import hashlib
import json
import logging
import re

from compiled_file_system import CompiledFileSystem
from file_system import FileNotFoundError
import third_party.json_schema_compiler.json_comment_eater as json_comment_eater
import third_party.json_schema_compiler.model as model
import url_constants

DEFAULT_ICON_PATH = '/images/sample-default-icon.png'

class SamplesDataSource(object):
  """Constructs a list of samples and their respective files and api calls.
  """
  class Factory(object):
    """A factory to create SamplesDataSource instances bound to individual
    Requests.
    """
    def __init__(self,
                 channel,
                 extensions_file_system,
                 apps_file_system,
                 ref_resolver_factory,
                 object_store_creator_factory,
                 extension_samples_path):
      self._svn_file_system = extensions_file_system
      self._github_file_system = apps_file_system
      self._static_path = '/%s/static' % channel
      self._ref_resolver = ref_resolver_factory.Create()
      self._extension_samples_path = extension_samples_path
      def create_compiled_fs(fs, fn, category):
        return CompiledFileSystem.Factory(
            fs,
            object_store_creator_factory).Create(fn,
                                                 SamplesDataSource,
                                                 category=category)
      self._extensions_cache = create_compiled_fs(extensions_file_system,
                                                  self._MakeSamplesList,
                                                  'extensions')
      self._apps_cache = create_compiled_fs(apps_file_system,
                                            lambda *args: self._MakeSamplesList(
                                                *args, is_apps=True),
                                            'apps')

    def Create(self, request):
      """Returns a new SamplesDataSource bound to |request|.
      """
      return SamplesDataSource(self._extensions_cache,
                               self._apps_cache,
                               self._extension_samples_path,
                               request)

    def _GetAPIItems(self, js_file):
      chrome_regex = '(chrome\.[a-zA-Z0-9\.]+)'
      calls = set(re.findall(chrome_regex, js_file))
      # Find APIs that have been assigned into variables.
      assigned_vars = dict(re.findall('var\s*([^\s]+)\s*=\s*%s;' % chrome_regex,
                                      js_file))
      # Replace the variable name with the full API name.
      for var_name, value in assigned_vars.iteritems():
        js_file = js_file.replace(var_name, value)
      return calls.union(re.findall(chrome_regex, js_file))

    def _GetDataFromManifest(self, path, file_system):
      manifest = file_system.ReadSingle(path + '/manifest.json')
      try:
        manifest_json = json.loads(json_comment_eater.Nom(manifest))
      except ValueError as e:
        logging.error('Error parsing manifest.json for %s: %s' % (path, e))
        return None
      l10n_data = {
        'name': manifest_json.get('name', ''),
        'description': manifest_json.get('description', None),
        'icon': manifest_json.get('icons', {}).get('128', None),
        'default_locale': manifest_json.get('default_locale', None),
        'locales': {}
      }
      if not l10n_data['default_locale']:
        return l10n_data
      locales_path = path + '/_locales/'
      locales_dir = file_system.ReadSingle(locales_path)
      if locales_dir:
        locales_files = file_system.Read(
            [locales_path + f + 'messages.json' for f in locales_dir]).Get()
        try:
          locales_json = [(locale_path, json.loads(contents))
                          for locale_path, contents in
                          locales_files.iteritems()]
        except ValueError as e:
          logging.error('Error parsing locales files for %s: %s' % (path, e))
        else:
          for path, json_ in locales_json:
            l10n_data['locales'][path[len(locales_path):].split('/')[0]] = json_
      return l10n_data

    def _MakeSamplesList(self, base_dir, files, is_apps=False):
      # HACK(kalman): The code here (for legacy reasons) assumes that |files| is
      # prefixed by |base_dir|, so make it true.
      files = ['%s%s' % (base_dir, f) for f in files]
      file_system = (self._github_file_system if is_apps else
                     self._svn_file_system)
      samples_list = []
      for filename in sorted(files):
        if filename.rsplit('/')[-1] != 'manifest.json':
          continue
        # This is a little hacky, but it makes a sample page.
        sample_path = filename.rsplit('/', 1)[-2]
        sample_files = [path for path in files
                        if path.startswith(sample_path + '/')]
        js_files = [path for path in sample_files if path.endswith('.js')]
        try:
          js_contents = file_system.Read(js_files).Get()
        except FileNotFoundError as e:
          logging.warning('Error fetching samples files: %s. Was a file '
                          'deleted from a sample? This warning should go away '
                          'in 5 minutes.' % e)
          continue
        api_items = set()
        for js in js_contents.values():
          api_items.update(self._GetAPIItems(js))

        api_calls = []
        for item in sorted(api_items):
          if len(item.split('.')) < 3:
            continue
          if item.endswith('.removeListener') or item.endswith('.hasListener'):
            continue
          if item.endswith('.addListener'):
            item = item[:-len('.addListener')]
          if item.startswith('chrome.'):
            item = item[len('chrome.'):]
          ref_data = self._ref_resolver.GetLink(item)
          if ref_data is None:
            continue
          api_calls.append({
            'name': ref_data['text'],
            'link': ref_data['href']
          })
        try:
          manifest_data = self._GetDataFromManifest(sample_path, file_system)
        except FileNotFoundError as e:
          logging.warning('Error getting data from samples manifest: %s. If '
                          'this file was deleted from a sample this message '
                          'should go away in 5 minutes.' % e)
          continue
        if manifest_data is None:
          continue

        sample_base_path = sample_path.split('/', 1)[1]
        if is_apps:
          url = url_constants.GITHUB_BASE + '/' + sample_base_path
          icon_base = url_constants.RAW_GITHUB_BASE + '/' + sample_base_path
          download_url = url
        else:
          url = sample_base_path
          icon_base = sample_base_path
          download_url = sample_base_path + '.zip'

        if manifest_data['icon'] is None:
          icon_path = self._static_path + DEFAULT_ICON_PATH
        else:
          icon_path = icon_base + '/' + manifest_data['icon']
        manifest_data.update({
          'icon': icon_path,
          'id': hashlib.md5(url).hexdigest(),
          'download_url': download_url,
          'url': url,
          'files': [f.replace(sample_path + '/', '') for f in sample_files],
          'api_calls': api_calls
        })
        samples_list.append(manifest_data)

      return samples_list

  def __init__(self,
               extensions_cache,
               apps_cache,
               extension_samples_path,
               request):
    self._extensions_cache = extensions_cache
    self._apps_cache = apps_cache
    self._extension_samples_path = extension_samples_path
    self._request = request

  def _GetAcceptedLanguages(self):
    accept_language = self._request.headers.get('Accept-Language', None)
    if accept_language is None:
      return []
    return [lang_with_q.split(';')[0].strip()
            for lang_with_q in accept_language.split(',')]

  def FilterSamples(self, key, api_name):
    """Fetches and filters the list of samples specified by |key|, returning
    only the samples that use the API |api_name|. |key| is either 'apps' or
    'extensions'.
    """
    api_search = api_name + '_'
    samples_list = []
    try:
      for sample in self.get(key):
        api_calls_unix = [model.UnixName(call['name'])
                          for call in sample['api_calls']]
        for call in api_calls_unix:
          if call.startswith(api_search):
            samples_list.append(sample)
            break
    except NotImplementedError:
      # If we're testing, the GithubFileSystem can't fetch samples.
      # Bug: http://crbug.com/141910
      return []
    return samples_list

  def _CreateSamplesDict(self, key):
    if key == 'apps':
      samples_list = self._apps_cache.GetFromFileListing('/')
    else:
      samples_list = self._extensions_cache.GetFromFileListing(
          self._extension_samples_path + '/')
    return_list = []
    for dict_ in samples_list:
      name = dict_['name']
      description = dict_['description']
      if description is None:
        description = ''
      if name.startswith('__MSG_') or description.startswith('__MSG_'):
        try:
          # Copy the sample dict so we don't change the dict in the cache.
          sample_data = dict_.copy()
          name_key = name[len('__MSG_'):-len('__')]
          description_key = description[len('__MSG_'):-len('__')]
          locale = sample_data['default_locale']
          for lang in self._GetAcceptedLanguages():
            if lang in sample_data['locales']:
              locale = lang
              break
          locale_data = sample_data['locales'][locale]
          sample_data['name'] = locale_data[name_key]['message']
          sample_data['description'] = locale_data[description_key]['message']
        except Exception as e:
          logging.error(e)
          # Revert the sample to the original dict.
          sample_data = dict_
        return_list.append(sample_data)
      else:
        return_list.append(dict_)
    return return_list

  def get(self, key):
    return {
      'apps': lambda: self._CreateSamplesDict('apps'),
      'extensions': lambda: self._CreateSamplesDict('extensions')
    }.get(key, lambda: {})()
