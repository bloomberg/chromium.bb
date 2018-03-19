#!/usr/bin/env python
#
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Process Android resources to generate R.java, and prepare for packaging.

This will crunch images with aapt2 and generate v14 compatible resources
(see generate_v14_compatible_resources.py).
"""

import codecs
import collections
import multiprocessing.pool
import optparse
import os
import re
import shutil
import subprocess
import sys
import zipfile
from xml.etree import ElementTree


import generate_v14_compatible_resources

from util import build_utils

_SOURCE_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(
    __file__))))
# Import jinja2 from third_party/jinja2
sys.path.insert(1, os.path.join(_SOURCE_ROOT, 'third_party'))
from jinja2 import Template # pylint: disable=F0401


_EMPTY_ANDROID_MANIFEST_PATH = os.path.join(
    _SOURCE_ROOT, 'build', 'android', 'AndroidManifest.xml')


# Represents a line from a R.txt file.
TextSymbolsEntry = collections.namedtuple('RTextEntry',
    ('java_type', 'resource_type', 'name', 'value'))


# A variation of this lists also exists in:
# //base/android/java/src/org/chromium/base/LocaleUtils.java
_CHROME_TO_ANDROID_LOCALE_MAP = {
    'en-GB': 'en-rGB',
    'en-US': 'en-rUS',
    'es-419': 'es-rUS',
    'fil': 'tl',
    'he': 'iw',
    'id': 'in',
    'pt-PT': 'pt-rPT',
    'pt-BR': 'pt-rBR',
    'yi': 'ji',
    'zh-CN': 'zh-rCN',
    'zh-TW': 'zh-rTW',
}

# Pngs that we shouldn't convert to webp. Please add rationale when updating.
_PNG_WEBP_BLACKLIST_PATTERN = re.compile('|'.join([
    # Crashes on Galaxy S5 running L (https://crbug.com/807059).
    r'.*star_gray\.png',
    # Android requires pngs for 9-patch images.
    r'.*\.9\.png',
    # Daydream (*.dd) requires pngs for icon files.
    r'.*\.dd\.png']))


class _ResourceWhitelist(object):
  def __init__(self, entries=None):
    self._entries = None
    if entries:
      self._entries = set(self._Key(x) for x in entries)

  def __contains__(self, entry):
    return self._entries is None or self._Key(entry) in self._entries

  @staticmethod
  def _Key(entry):
    # Whitelists should only care about the name of the resource rather than the
    # resource ID (since the whitelist is from another compilation unit, the
    # resource IDs may not match).
    return (entry.java_type, entry.resource_type, entry.name)


def _ParseArgs(args):
  """Parses command line options.

  Returns:
    An options object as from optparse.OptionsParser.parse_args()
  """
  parser = optparse.OptionParser()
  build_utils.AddDepfileOption(parser)

  parser.add_option('--android-sdk-jar',
                    help='the path to android jar file.')
  parser.add_option('--aapt-path',
                    help='path to the Android aapt tool')
  parser.add_option('--non-constant-id', action='store_true')

  parser.add_option('--android-manifest', help='AndroidManifest.xml path')
  parser.add_option('--custom-package', help='Java package for R.java')
  parser.add_option(
      '--shared-resources',
      action='store_true',
      help='Make a resource package that can be loaded by a different'
      'application at runtime to access the package\'s resources.')
  parser.add_option(
      '--app-as-shared-lib',
      action='store_true',
      help='Make a resource package that can be loaded as shared library.')
  parser.add_option(
      '--shared-resources-whitelist',
      help='An R.txt file acting as a whitelist for resources that should be '
      'non-final and have their package ID changed at runtime in R.java. If no '
      'whitelist is provided, then everything is whitelisted.')

  parser.add_option('--resource-dirs',
                    default='[]',
                    help='Directories containing resources of this target.')
  parser.add_option('--dependencies-res-zips',
                    help='Resources from dependents.')

  parser.add_option('--resource-zip-out',
                    help='Path for output zipped resources.')

  parser.add_option('--srcjar-out',
                    help='Path to srcjar to contain generated R.java.')
  parser.add_option('--r-text-out',
                    help='Path to store the generated R.txt file.')
  parser.add_option('--r-text-in',
                    help='Path to pre-existing R.txt for these resources. '
                    'Resource names from it will be used to generate R.java '
                    'instead of aapt-generated R.txt.')

  parser.add_option('--proguard-file',
                    help='Path to proguard.txt generated file')
  parser.add_option('--proguard-file-main-dex',
                    help='Path to proguard.txt generated file for main dex')

  parser.add_option(
      '--v14-skip',
      action="store_true",
      help='Do not generate nor verify v14 resources')

  parser.add_option(
      '--extra-res-packages',
      help='Additional package names to generate R.java files for')
  parser.add_option(
      '--extra-r-text-files',
      help='For each additional package, the R.txt file should contain a '
      'list of resources to be included in the R.java file in the format '
      'generated by aapt')

  parser.add_option('--support-zh-hk', action='store_true',
                    help='Use zh-rTW resources for zh-rHK.')

  parser.add_option('--stamp', help='File to touch on success')

  parser.add_option('--debuggable',
                    action='store_true',
                    help='Whether to add android:debuggable="true"')
  parser.add_option('--version-code', help='Version code for apk.')
  parser.add_option('--version-name', help='Version name for apk.')
  parser.add_option('--no-compress', help='disables compression for the '
                    'given comma separated list of extensions')
  parser.add_option('--locale-whitelist',
                    default='[]',
                    help='GN list of languages to include. All other language '
                         'configs will be stripped out. List may include '
                         'a combination of Android locales or Chrome locales.')
  parser.add_option('--apk-path',
                    help='Path to output (partial) apk.')
  parser.add_option('--exclude-xxxhdpi', action='store_true',
                    help='Do not include xxxhdpi drawables.')
  parser.add_option('--xxxhdpi-whitelist',
                    default='[]',
                    help='GN list of globs that say which xxxhdpi images to '
                         'include even when --exclude-xxxhdpi is set.')
  parser.add_option('--png-to-webp', action='store_true',
                    help='Convert png files to webp format.')
  parser.add_option('--webp-binary', default='',
                    help='Path to the cwebp binary.')
  parser.add_option('--no-xml-namespaces',
                    action='store_true',
                    help='Whether to strip xml namespaces from processed xml '
                    'resources')

  options, positional_args = parser.parse_args(args)

  if positional_args:
    parser.error('No positional arguments should be given.')

  # Check that required options have been provided.
  required_options = (
      'android_sdk_jar',
      'aapt_path',
      'dependencies_res_zips',
      )
  build_utils.CheckOptions(options, parser, required=required_options)

  options.resource_dirs = build_utils.ParseGnList(options.resource_dirs)
  options.dependencies_res_zips = (
      build_utils.ParseGnList(options.dependencies_res_zips))

  options.locale_whitelist = build_utils.ParseGnList(options.locale_whitelist)
  options.xxxhdpi_whitelist = build_utils.ParseGnList(options.xxxhdpi_whitelist)

  # Don't use [] as default value since some script explicitly pass "".
  if options.extra_res_packages:
    options.extra_res_packages = (
        build_utils.ParseGnList(options.extra_res_packages))
  else:
    options.extra_res_packages = []

  if options.extra_r_text_files:
    options.extra_r_text_files = (
        build_utils.ParseGnList(options.extra_r_text_files))
  else:
    options.extra_r_text_files = []

  return options


def _CreateRJavaFiles(srcjar_dir, main_r_txt_file, packages, r_txt_files,
      shared_resources, non_constant_id, whitelist_r_txt_file, is_apk):
  assert len(packages) == len(r_txt_files), 'Need one R.txt file per package'

  # Map of (resource_type, name) -> Entry.
  # Contains the correct values for resources.
  all_resources = {}
  for entry in _ParseTextSymbolsFile(main_r_txt_file):
    entry = entry._replace(value=_FixPackageIds(entry.value))
    all_resources[(entry.resource_type, entry.name)] = entry

  if whitelist_r_txt_file:
    whitelisted_resources = _ResourceWhitelist(
        _ParseTextSymbolsFile(whitelist_r_txt_file))
  else:
    whitelisted_resources = _ResourceWhitelist()

  # Map of package_name->resource_type->entry
  resources_by_package = (
      collections.defaultdict(lambda: collections.defaultdict(list)))
  # Build the R.java files using each package's R.txt file, but replacing
  # each entry's placeholder value with correct values from all_resources.
  for package, r_txt_file in zip(packages, r_txt_files):
    if package in resources_by_package:
      raise Exception(('Package name "%s" appeared twice. All '
                       'android_resources() targets must use unique package '
                       'names, or no package name at all.') % package)
    resources_by_type = resources_by_package[package]
    # The sub-R.txt files have the wrong values at this point. Read them to
    # figure out which entries belong to them, but use the values from the
    # main R.txt file.
    for entry in _ParseTextSymbolsFile(r_txt_file):
      entry = all_resources.get((entry.resource_type, entry.name))
      # For most cases missing entry here is an error. It means that some
      # library claims to have or depend on a resource that isn't included into
      # the APK. There is one notable exception: Google Play Services (GMS).
      # GMS is shipped as a bunch of AARs. One of them - basement - contains
      # R.txt with ids of all resources, but most of the resources are in the
      # other AARs. However, all other AARs reference their resources via
      # basement's R.java so the latter must contain all ids that are in its
      # R.txt. Most targets depend on only a subset of GMS AARs so some
      # resources are missing, which is okay because the code that references
      # them is missing too. We can't get an id for a resource that isn't here
      # so the only solution is to skip the resource entry entirely.
      #
      # We can verify that all entries referenced in the code were generated
      # correctly by running Proguard on the APK: it will report missing
      # fields.
      if entry:
        resources_by_type[entry.resource_type].append(entry)

  for package, resources_by_type in resources_by_package.iteritems():
    package_r_java_dir = os.path.join(srcjar_dir, *package.split('.'))
    build_utils.MakeDirectory(package_r_java_dir)
    package_r_java_path = os.path.join(package_r_java_dir, 'R.java')
    java_file_contents = _CreateRJavaFile(package, resources_by_type,
        shared_resources, non_constant_id, whitelisted_resources, is_apk)
    with open(package_r_java_path, 'w') as f:
      f.write(java_file_contents)


def _ParseTextSymbolsFile(path):
  """Given an R.txt file, returns a list of TextSymbolsEntry."""
  ret = []
  with open(path) as f:
    for line in f:
      m = re.match(r'(int(?:\[\])?) (\w+) (\w+) (.+)$', line)
      if not m:
        raise Exception('Unexpected line in R.txt: %s' % line)
      java_type, resource_type, name, value = m.groups()
      ret.append(TextSymbolsEntry(java_type, resource_type, name, value))
  return ret


def _FixPackageIds(resource_value):
  # Resource IDs for resources belonging to regular APKs have their first byte
  # as 0x7f (package id). However with webview, since it is not a regular apk
  # but used as a shared library, aapt is passed the --shared-resources flag
  # which changes some of the package ids to 0x02 and 0x00.  This function just
  # normalises all package ids to 0x7f, which the generated code in R.java
  # changes to the correct package id at runtime.
  # resource_value is a string with either, a single value '0x12345678', or an
  # array of values like '{ 0xfedcba98, 0x01234567, 0x56789abc }'
  return re.sub(r'0x(?!01)\d\d', r'0x7f', resource_value)


def _CreateRJavaFile(package, resources_by_type, shared_resources,
                     non_constant_id, whitelisted_resources, is_apk):
  """Generates the contents of a R.java file."""
  final_resources_by_type = collections.defaultdict(list)
  non_final_resources_by_type = collections.defaultdict(list)
  if shared_resources or non_constant_id:
    for res_type, resources in resources_by_type.iteritems():
      for entry in resources:
        # Entries in stylable that are not int[] are not actually resource ids
        # but constants. If we are creating an apk there is no reason for them
        # to be non-final. However for libraries, they may be clobbered later on
        # and thus should remain non-final. This is regardless of the
        # whitelisting rules (since they are not actually resources).
        if entry.resource_type == 'styleable' and entry.java_type != 'int[]':
          if is_apk:
            final_resources_by_type[res_type].append(entry)
          else:
            non_final_resources_by_type[res_type].append(entry)
        elif entry in whitelisted_resources:
          non_final_resources_by_type[res_type].append(entry)
        else:
          final_resources_by_type[res_type].append(entry)
  else:
    final_resources_by_type = resources_by_type

  # Keep these assignments all on one line to make diffing against regular
  # aapt-generated files easier.
  create_id = ('{{ e.resource_type }}.{{ e.name }} ^= packageIdTransform;')
  # Here we diverge from what aapt does. Because we have so many
  # resources, the onResourcesLoaded method was exceeding the 64KB limit that
  # Java imposes. For this reason we split onResourcesLoaded into different
  # methods for each resource type.
  template = Template("""/* AUTO-GENERATED FILE.  DO NOT MODIFY. */

package {{ package }};

public final class R {
    private static boolean sResourcesDidLoad;
    {% for resource_type in resource_types %}
    public static final class {{ resource_type }} {
        {% for e in final_resources[resource_type] %}
        public static final {{ e.java_type }} {{ e.name }} = {{ e.value }};
        {% endfor %}
        {% for e in non_final_resources[resource_type] %}
        public static {{ e.java_type }} {{ e.name }} = {{ e.value }};
        {% endfor %}
    }
    {% endfor %}
    {% if shared_resources %}
    public static void transfromArray(int[] array, int packageIdTransform) {
        for (int i=0; i < array.length; i++) {
            if ((array[i] >>> 24) == 0x7f) {
                array[i] ^= packageIdTransform;
            }
        }
    }
    public static void onResourcesLoaded(int packageId) {
        assert !sResourcesDidLoad;
        sResourcesDidLoad = true;
        int packageIdTransform = (packageId ^ 0x7f) << 24;
        {% for resource_type in resource_types %}
        onResourcesLoaded{{ resource_type|title }}(packageIdTransform);
        {% for e in non_final_resources[resource_type] %}
        {% if e.java_type == 'int[]' %}
        transfromArray({{ e.resource_type }}.{{ e.name }}, packageIdTransform);
        {% endif %}
        {% endfor %}
        {% endfor %}
    }
    {% for res_type in resource_types %}
    private static void onResourcesLoaded{{ res_type|title }} (
            int packageIdTransform) {
        {% for e in non_final_resources[res_type] %}
        {% if res_type != 'styleable' and e.java_type != 'int[]' %}
        """ + create_id + """
        {% endif %}
        {% endfor %}
    }
    {% endfor %}
    {% endif %}
}
""", trim_blocks=True, lstrip_blocks=True)

  return template.render(package=package,
                         resource_types=sorted(resources_by_type),
                         shared_resources=shared_resources,
                         final_resources=final_resources_by_type,
                         non_final_resources=non_final_resources_by_type)


def _GenerateGlobs(pattern):
  # This function processes the aapt ignore assets pattern into a list of globs
  # to be used to exclude files on the python side. It removes the '!', which is
  # used by aapt to mean 'not chatty' so it does not output if the file is
  # ignored (we dont output anyways, so it is not required). This function does
  # not handle the <dir> and <file> prefixes used by aapt and are assumed not to
  # be included in the pattern string.
  return pattern.replace('!', '').split(':')


def _ZipResources(resource_dirs, zip_path, ignore_pattern):
  # Python zipfile does not provide a way to replace a file (it just writes
  # another file with the same name). So, first collect all the files to put
  # in the zip (with proper overriding), and then zip them.
  # ignore_pattern is a string of ':' delimited list of globs used to ignore
  # files that should not be part of the final resource zip.
  files_to_zip = dict()
  globs = _GenerateGlobs(ignore_pattern)
  for d in resource_dirs:
    for root, _, files in os.walk(d):
      for f in files:
        archive_path = f
        parent_dir = os.path.relpath(root, d)
        if parent_dir != '.':
          archive_path = os.path.join(parent_dir, f)
        path = os.path.join(root, f)
        if build_utils.MatchesGlob(archive_path, globs):
          continue
        files_to_zip[archive_path] = path
  build_utils.DoZip(files_to_zip.iteritems(), zip_path)

def _SortZip(original_path, sorted_path):
  with zipfile.ZipFile(sorted_path, 'w') as sorted_zip, \
      zipfile.ZipFile(original_path, 'r') as original_zip:
    for info in sorted(original_zip.infolist(), key=lambda i: i.filename):
      sorted_zip.writestr(info, original_zip.read(info))



def _DuplicateZhResources(resource_dirs):
  for resource_dir in resource_dirs:
    # We use zh-TW resources for zh-HK (if we have zh-TW resources).
    for path in build_utils.IterFiles(resource_dir):
      if 'zh-rTW' in path:
        hk_path = path.replace('zh-rTW', 'zh-rHK')
        build_utils.MakeDirectory(os.path.dirname(hk_path))
        shutil.copyfile(path, hk_path)

def _ExtractPackageFromManifest(manifest_path):
  doc = ElementTree.parse(manifest_path)
  return doc.getroot().get('package')


def _ToAaptLocales(locale_whitelist, support_zh_hk):
  """Converts the list of Chrome locales to aapt config locales."""
  ret = set()
  for locale in locale_whitelist:
    locale = _CHROME_TO_ANDROID_LOCALE_MAP.get(locale, locale)
    if locale is None or ('-' in locale and '-r' not in locale):
      raise Exception('_CHROME_TO_ANDROID_LOCALE_MAP needs updating.'
                      ' Found: %s' % locale)
    ret.add(locale)
    # Always keep non-regional fall-backs.
    language = locale.split('-')[0]
    ret.add(language)

  # We don't actually support zh-HK in Chrome on Android, but we mimic the
  # native side behavior where we use zh-TW resources when the locale is set to
  # zh-HK. See https://crbug.com/780847.
  if support_zh_hk:
    assert not any('HK' in l for l in locale_whitelist), (
        'Remove special logic if zh-HK is now supported (crbug.com/780847).')
    ret.add('zh-rHK')
  return sorted(ret)


def _MoveImagesToNonMdpiFolders(res_root):
  """Move images from drawable-*-mdpi-* folders to drawable-* folders.

  Why? http://crbug.com/289843
  """
  for src_dir_name in os.listdir(res_root):
    src_components = src_dir_name.split('-')
    if src_components[0] != 'drawable' or 'mdpi' not in src_components:
      continue
    src_dir = os.path.join(res_root, src_dir_name)
    if not os.path.isdir(src_dir):
      continue
    dst_components = [c for c in src_components if c != 'mdpi']
    assert dst_components != src_components
    dst_dir_name = '-'.join(dst_components)
    dst_dir = os.path.join(res_root, dst_dir_name)
    build_utils.MakeDirectory(dst_dir)
    for src_file_name in os.listdir(src_dir):
      if not os.path.splitext(src_file_name)[1] in ('.png', '.webp'):
        continue
      src_file = os.path.join(src_dir, src_file_name)
      dst_file = os.path.join(dst_dir, src_file_name)
      assert not os.path.lexists(dst_file)
      shutil.move(src_file, dst_file)


def _CreateLinkApkArgs(options):
  link_command = [
    options.aapt_path + '2',
    'link',
    '--version-code', options.version_code,
    '--version-name', options.version_name,
    '--auto-add-overlay',
    '--no-version-vectors',
    '-I', options.android_sdk_jar,
    '-o', options.apk_path,
  ]

  if options.proguard_file:
    link_command += ['--proguard', options.proguard_file]
  if options.proguard_file_main_dex:
    link_command += ['--proguard-main-dex', options.proguard_file_main_dex]

  if options.no_compress:
    for ext in options.no_compress.split(','):
      link_command += ['-0', ext]

  if options.shared_resources:
    link_command.append('--shared-lib')

  if options.locale_whitelist:
    aapt_locales = _ToAaptLocales(
        options.locale_whitelist, options.support_zh_hk)
    link_command += ['-c', ','.join(aapt_locales)]

  if options.no_xml_namespaces:
    link_command.append('--no-xml-namespaces')

  return link_command


def _ExtractVersionFromSdk(aapt_path, sdk_path):
  output = subprocess.check_output([aapt_path, 'dump', 'badging', sdk_path])
  version_code = re.search(r"versionCode='(.*?)'", output).group(1)
  version_name = re.search(r"versionName='(.*?)'", output).group(1)
  return version_code, version_name,


def _FixManifest(options, temp_dir):
  debug_manifest_path = os.path.join(temp_dir, 'AndroidManifest.xml')
  _ANDROID_NAMESPACE = 'http://schemas.android.com/apk/res/android'
  _TOOLS_NAMESPACE = 'http://schemas.android.com/tools'
  ElementTree.register_namespace('android', _ANDROID_NAMESPACE)
  ElementTree.register_namespace('tools', _TOOLS_NAMESPACE)
  original_manifest = ElementTree.parse(options.android_manifest)

  version_code, version_name = _ExtractVersionFromSdk(
      options.aapt_path, options.android_sdk_jar)

  # ElementTree.find does not work if the required tag is the root.
  if original_manifest.getroot().tag == 'manifest':
    manifest_node = original_manifest.getroot()
  else:
    manifest_node = original_manifest.find('manifest')

  manifest_node.set('platformBuildVersionCode', version_code)
  manifest_node.set('platformBuildVersionName', version_name)

  if options.debuggable:
    app_node = original_manifest.find('application')
    app_node.set('{%s}%s' % (_ANDROID_NAMESPACE, 'debuggable'), 'true')

  with open(debug_manifest_path, 'w') as debug_manifest:
    debug_manifest.write(ElementTree.tostring(
        original_manifest.getroot(), encoding='UTF-8'))

  return debug_manifest_path


def _ResourceNameFromPath(path):
  return os.path.splitext(os.path.basename(path))[0]


def _CreateKeepPredicate(resource_dirs, exclude_xxxhdpi, xxxhdpi_whitelist):
  if not exclude_xxxhdpi:
    # Do not extract dotfiles (e.g. ".gitkeep"). aapt ignores them anyways.
    return lambda path: os.path.basename(path)[0] != '.'

  # Returns False only for xxxhdpi non-mipmap, non-whitelisted drawables.
  naive_predicate = lambda path: (
      not re.search(r'[/-]xxxhdpi[/-]', path) or
      re.search(r'[/-]mipmap[/-]', path) or
      build_utils.MatchesGlob(path, xxxhdpi_whitelist))

  # Build a set of all non-xxxhdpi drawables to ensure that we never exclude any
  # xxxhdpi drawable that does not exist in other densities.
  non_xxxhdpi_drawables = set()
  for resource_dir in resource_dirs:
    for path in build_utils.IterFiles(resource_dir):
      if re.search(r'[/-]drawable[/-]', path) and naive_predicate(path):
        non_xxxhdpi_drawables.add(_ResourceNameFromPath(path))

  return lambda path: (naive_predicate(path) or
                       _ResourceNameFromPath(path) not in non_xxxhdpi_drawables)


def _ConvertToWebP(webp_binary, png_files):
  pool = multiprocessing.pool.ThreadPool(10)
  def convert_image(png_path):
    root = os.path.splitext(png_path)[0]
    webp_path = root + '.webp'
    args = [webp_binary, png_path, '-mt', '-quiet', '-m', '6', '-q', '100',
        '-lossless', '-o', webp_path]
    subprocess.check_call(args)
    os.remove(png_path)

  pool.map(convert_image, [f for f in png_files
                           if not _PNG_WEBP_BLACKLIST_PATTERN.match(f)])
  pool.close()
  pool.join()


def _CompileDeps(aapt_path, dep_subdirs, temp_dir):
  partials_dir = os.path.join(temp_dir, 'partials')
  build_utils.MakeDirectory(partials_dir)
  partial_compile_command = [
      aapt_path + '2',
      'compile',
      # TODO(wnwen): Turn this on once aapt2 forces 9-patch to be crunched.
      # '--no-crunch',
  ]
  pool = multiprocessing.pool.ThreadPool(10)
  def compile_partial(directory):
    dirname = os.path.basename(directory)
    partial_path = os.path.join(partials_dir, dirname + '.zip')
    compile_command = (partial_compile_command +
                       ['--dir', directory, '-o', partial_path])
    build_utils.CheckOutput(compile_command)

    # Sorting the files in the partial ensures deterministic output from the
    # aapt2 link step which uses order of files in the partial.
    sorted_partial_path = os.path.join(partials_dir, dirname + '.sorted.zip')
    _SortZip(partial_path, sorted_partial_path)

    return sorted_partial_path

  partials = pool.map(compile_partial, dep_subdirs)
  pool.close()
  pool.join()
  return partials


def _PackageApk(options, dep_subdirs, temp_dir, gen_dir, r_txt_path):
  _DuplicateZhResources(dep_subdirs)

  keep_predicate = _CreateKeepPredicate(
      dep_subdirs, options.exclude_xxxhdpi, options.xxxhdpi_whitelist)
  png_paths = []
  for directory in dep_subdirs:
    for f in build_utils.IterFiles(directory):
      if not keep_predicate(f):
        os.remove(f)
      elif f.endswith('.png'):
        png_paths.append(f)
  if png_paths and options.png_to_webp:
    _ConvertToWebP(options.webp_binary, png_paths)
  for directory in dep_subdirs:
    _MoveImagesToNonMdpiFolders(directory)

  link_command = _CreateLinkApkArgs(options)
  link_command += ['--output-text-symbols', r_txt_path]
  link_command += ['--java', gen_dir]

  fixed_manifest = _FixManifest(options, temp_dir)
  link_command += ['--manifest', fixed_manifest]

  partials = _CompileDeps(options.aapt_path, dep_subdirs, temp_dir)
  for partial in partials:
    link_command += ['-R', partial]

  # Creates a .zip with AndroidManifest.xml, resources.arsc, res/*
  # Also creates R.txt
  build_utils.CheckOutput(
      link_command, print_stdout=False, print_stderr=False)


# _PackageLibrary uses aapt rather than aapt2 because aapt2 compile does not
# support outputting an R.txt file.
def _PackageLibrary(options, dep_subdirs, temp_dir, gen_dir):
  v14_dir = os.path.join(temp_dir, 'v14')
  build_utils.MakeDirectory(v14_dir)

  # Generate R.java. This R.java contains non-final constants and is used only
  # while compiling the library jar (e.g. chromium_content.jar). When building
  # an apk, a new R.java file with the correct resource -> ID mappings will be
  # generated by merging the resources from all libraries and the main apk
  # project.
  package_command = [options.aapt_path,
                     'package',
                     '-m',
                     '-M', _EMPTY_ANDROID_MANIFEST_PATH,
                     '--no-crunch',
                     '--auto-add-overlay',
                     '--no-version-vectors',
                     '-I', options.android_sdk_jar,
                     '--output-text-symbols', gen_dir,
                     '-J', gen_dir,  # Required for R.txt generation.
                     '--ignore-assets', build_utils.AAPT_IGNORE_PATTERN]

  # Adding all dependencies as sources is necessary for @type/foo references
  # to symbols within dependencies to resolve. However, it has the side-effect
  # that all Java symbols from dependencies are copied into the new R.java.
  # E.g.: It enables an arguably incorrect usage of
  # "mypackage.R.id.lib_symbol" where "libpackage.R.id.lib_symbol" would be
  # more correct. This is just how Android works.
  for d in dep_subdirs:
    package_command += ['-S', d]

  input_resource_dirs = options.resource_dirs

  for d in input_resource_dirs:
    package_command += ['-S', d]

  if not options.v14_skip:
    for resource_dir in input_resource_dirs:
      generate_v14_compatible_resources.GenerateV14Resources(
          resource_dir,
          v14_dir)

  # This is the list of directories with resources to put in the final .zip
  zip_resource_dirs = input_resource_dirs + [v14_dir]

  if options.resource_zip_out:
    _ZipResources(zip_resource_dirs, options.resource_zip_out,
                  build_utils.AAPT_IGNORE_PATTERN)

  # Only creates an R.txt
  build_utils.CheckOutput(
      package_command, print_stdout=False, print_stderr=False)


def _CreateRTxtAndSrcJar(options, r_txt_path, srcjar_dir):
  # When an empty res/ directory is passed, aapt does not write an R.txt.
  if not os.path.exists(r_txt_path):
    build_utils.Touch(r_txt_path)

  if options.r_text_in:
    r_txt_path = options.r_text_in

  packages = list(options.extra_res_packages)
  r_txt_files = list(options.extra_r_text_files)

  cur_package = options.custom_package
  if not options.custom_package and options.android_manifest:
    cur_package = _ExtractPackageFromManifest(options.android_manifest)

  # Don't create a .java file for the current resource target when:
  # - no package name was provided (either by manifest or build rules),
  # - there was already a dependent android_resources() with the same
  #   package (occurs mostly when an apk target and resources target share
  #   an AndroidManifest.xml)
  if cur_package and cur_package not in packages:
    packages.append(cur_package)
    r_txt_files.append(r_txt_path)

  if packages:
    shared_resources = options.shared_resources or options.app_as_shared_lib
    _CreateRJavaFiles(srcjar_dir, r_txt_path, packages, r_txt_files,
        shared_resources, options.non_constant_id,
        options.shared_resources_whitelist, bool(options.apk_path))

  if options.srcjar_out:
    build_utils.ZipDir(options.srcjar_out, srcjar_dir)

  if options.r_text_out:
    shutil.copyfile(r_txt_path, options.r_text_out)


def _ExtractDeps(dep_zips, deps_dir):
  dep_subdirs = []
  for z in dep_zips:
    subdir = os.path.join(deps_dir, os.path.basename(z))
    if os.path.exists(subdir):
      raise Exception('Resource zip name conflict: ' + os.path.basename(z))
    build_utils.ExtractAll(z, path=subdir)
    dep_subdirs.append(subdir)
  return dep_subdirs


def _OnStaleMd5(options):
  with build_utils.TempDir() as temp_dir:
    deps_dir = os.path.join(temp_dir, 'deps')
    build_utils.MakeDirectory(deps_dir)
    gen_dir = os.path.join(temp_dir, 'gen')
    build_utils.MakeDirectory(gen_dir)
    r_txt_path = os.path.join(gen_dir, 'R.txt')
    srcjar_dir = os.path.join(temp_dir, 'java')

    dep_subdirs = _ExtractDeps(options.dependencies_res_zips, deps_dir)

    if options.apk_path:
      _PackageApk(options, dep_subdirs, temp_dir, gen_dir, r_txt_path)
    else:
      _PackageLibrary(options, dep_subdirs, temp_dir, gen_dir)

    _CreateRTxtAndSrcJar(options, r_txt_path, srcjar_dir)


def main(args):
  args = build_utils.ExpandFileArgs(args)
  options = _ParseArgs(args)

  # Order of these must match order specified in GN so that the correct one
  # appears first in the depfile.
  possible_output_paths = [
    options.apk_path,
    options.resource_zip_out,
    options.r_text_out,
    options.srcjar_out,
    options.proguard_file,
    options.proguard_file_main_dex,
  ]
  output_paths = [x for x in possible_output_paths if x]

  # List python deps in input_strings rather than input_paths since the contents
  # of them does not change what gets written to the depsfile.
  input_strings = options.extra_res_packages + [
    options.app_as_shared_lib,
    options.custom_package,
    options.non_constant_id,
    options.shared_resources,
    options.v14_skip,
    options.exclude_xxxhdpi,
    options.xxxhdpi_whitelist,
    str(options.debuggable),
    str(options.png_to_webp),
    str(options.support_zh_hk),
    str(options.no_xml_namespaces),
  ]

  if options.apk_path:
    input_strings.extend(_CreateLinkApkArgs(options))

  possible_input_paths = [
    options.aapt_path,
    options.android_manifest,
    options.android_sdk_jar,
    options.shared_resources_whitelist,
  ]
  input_paths = [x for x in possible_input_paths if x]
  input_paths.extend(options.dependencies_res_zips)
  input_paths.extend(options.extra_r_text_files)

  if options.webp_binary:
    input_paths.append(options.webp_binary)

  # Resource files aren't explicitly listed in GN. Listing them in the depfile
  # ensures the target will be marked stale when resource files are removed.
  depfile_deps = []
  resource_names = []
  for resource_dir in options.resource_dirs:
    for resource_file in build_utils.FindInDirectory(resource_dir, '*'):
      # Don't list the empty .keep file in depfile. Since it doesn't end up
      # included in the .zip, it can lead to -w 'dupbuild=err' ninja errors
      # if ever moved.
      if not resource_file.endswith(os.path.join('empty', '.keep')):
        input_paths.append(resource_file)
        depfile_deps.append(resource_file)
      resource_names.append(os.path.relpath(resource_file, resource_dir))

  # Resource filenames matter to the output, so add them to strings as well.
  # This matters if a file is renamed but not changed (http://crbug.com/597126).
  input_strings.extend(sorted(resource_names))

  build_utils.CallAndWriteDepfileIfStale(
      lambda: _OnStaleMd5(options),
      options,
      input_paths=input_paths,
      input_strings=input_strings,
      output_paths=output_paths,
      depfile_deps=depfile_deps)


if __name__ == '__main__':
  main(sys.argv[1:])
