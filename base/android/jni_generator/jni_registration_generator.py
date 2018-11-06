#!/usr/bin/env python
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Generate JNI registration entry points

Creates a header file with two static functions: RegisterMainDexNatives() and
RegisterNonMainDexNatives(). Together, these will use manual JNI registration
to register all native methods that exist within an application."""

import argparse
import jni_generator
import multiprocessing
import os
import string
import sys
from util import build_utils


# All but FULL_CLASS_NAME, which is used only for sorting.
MERGEABLE_KEYS = [
    'CLASS_PATH_DECLARATIONS',
    'FORWARD_DECLARATIONS',
    'JNI_NATIVE_METHOD',
    'JNI_NATIVE_METHOD_ARRAY',
    'PROXY_NATIVE_METHOD_ARRAY',
    'PROXY_NATIVE_METHOD_ARRAY_MAIN_DEX',
    'REGISTER_MAIN_DEX_NATIVES',
    'REGISTER_NON_MAIN_DEX_NATIVES',
]


def GenerateJNIHeader(java_file_paths, output_file, args):
  """Generate a header file including two registration functions.

  Forward declares all JNI registration functions created by jni_generator.py.
  Calls the functions in RegisterMainDexNatives() if they are main dex. And
  calls them in RegisterNonMainDexNatives() if they are non-main dex.

  Args:
      java_file_paths: A list of java file paths.
      output_file: A relative path to output file.
      args: All input arguments.
  """
  # Without multiprocessing, script takes ~13 seconds for chrome_public_apk
  # on a z620. With multiprocessing, takes ~2 seconds.
  pool = multiprocessing.Pool()
  paths = (p for p in java_file_paths if p not in args.no_register_java)
  results = [d for d in pool.imap_unordered(_DictForPath, paths) if d]
  pool.close()

  # Sort to make output deterministic.
  results.sort(key=lambda d: d['FULL_CLASS_NAME'])

  combined_dict = {}
  for key in MERGEABLE_KEYS:
    combined_dict[key] = ''.join(d.get(key, '') for d in results)

  combined_dict['HEADER_GUARD'] = \
      os.path.splitext(output_file)[0].replace('/', '_').upper() + '_'
  combined_dict['NAMESPACE'] = args.namespace

  header_content = CreateFromDict(combined_dict)
  if output_file:
    jni_generator.WriteOutput(output_file, header_content)
  else:
    print header_content


def _DictForPath(path):
  with open(path) as f:
    contents = jni_generator.RemoveComments(f.read())
    if '@JniIgnoreNatives' in contents:
      return None

  fully_qualified_class = jni_generator.ExtractFullyQualifiedJavaClassName(
      path, contents)
  natives = jni_generator.ExtractNatives(contents, 'long')

  natives += jni_generator.NativeProxyHelpers.ExtractStaticProxyNatives(
      fully_qualified_class=fully_qualified_class,
      contents=contents,
      ptr_type='long')
  if len(natives) == 0:
    return None
  namespace = jni_generator.ExtractJNINamespace(contents)
  jni_params = jni_generator.JniParams(fully_qualified_class)
  jni_params.ExtractImportsAndInnerClasses(contents)
  is_main_dex = jni_generator.IsMainDexJavaClass(contents)
  header_generator = HeaderGenerator(namespace, fully_qualified_class, natives,
                                     jni_params, is_main_dex)
  return header_generator.Generate()

def _SetProxyRegistrationFields(registration_dict):
  registration_template = string.Template("""\

static const JNINativeMethod kMethods_${ESCAPED_PROXY_CLASS}[] = {
${KMETHODS}
};

JNI_REGISTRATION_EXPORT bool ${REGISTRATION_NAME}(JNIEnv* env) {
  const int number_of_methods = arraysize(kMethods_${ESCAPED_PROXY_CLASS});

  base::android::ScopedJavaLocalRef<jclass> native_clazz = base::android::GetClass(env, "${PROXY_CLASS}");
  if (env->RegisterNatives(
      native_clazz.obj(),
      kMethods_${ESCAPED_PROXY_CLASS},
      number_of_methods) < 0) {

    jni_generator::HandleRegistrationError(env, native_clazz.obj(), __FILE__);
    return false;
  }

  return true;
}
""")

  registration_call = string.Template("""\

  // Register natives in a proxy.
  if (!${REGISTRATION_NAME}(env)) {
    return false;
  }
""")

  sub_dict = {
      'ESCAPED_PROXY_CLASS':
          jni_generator.NativeProxyHelpers.ESCAPED_NATIVE_PROXY_CLASS,
      'PROXY_CLASS':
          jni_generator.NATIVE_PROXY_QUALIFIED_NAME,
      'KMETHODS':
          registration_dict['PROXY_NATIVE_METHOD_ARRAY'],
      'REGISTRATION_NAME':
          jni_generator.GetRegistrationFunctionName(
              jni_generator.NATIVE_PROXY_QUALIFIED_NAME)
  }

  if registration_dict['PROXY_NATIVE_METHOD_ARRAY']:
    proxy_native_array = registration_template.substitute(sub_dict)
    proxy_natives_registration = registration_call.substitute(sub_dict)
  else:
    proxy_native_array = ''
    proxy_natives_registration = ''

  if registration_dict['PROXY_NATIVE_METHOD_ARRAY_MAIN_DEX']:
    sub_dict['REGISTRATION_NAME'] += 'MAIN_DEX'
    sub_dict['ESCAPED_PROXY_CLASS'] += 'MAIN_DEX'
    sub_dict['KMETHODS'] = (
        registration_dict['PROXY_NATIVE_METHOD_ARRAY_MAIN_DEX'])
    proxy_native_array += registration_template.substitute(sub_dict)
    main_dex_call = registration_call.substitute(sub_dict)
  else:
    main_dex_call = ''

  registration_dict['PROXY_NATIVE_METHOD_ARRAY'] = proxy_native_array
  registration_dict['REGISTER_PROXY_NATIVES'] = proxy_natives_registration
  registration_dict['REGISTER_MAIN_DEX_PROXY_NATIVES'] = main_dex_call


def CreateFromDict(registration_dict):
  """Returns the content of the header file."""

  template = string.Template("""\
// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


// This file is autogenerated by
//     base/android/jni_generator/jni_registration_generator.py
// Please do not change its content.

#ifndef ${HEADER_GUARD}
#define ${HEADER_GUARD}

#include <jni.h>

#include "base/android/jni_generator/jni_generator_helper.h"
#include "base/android/jni_int_wrapper.h"


// Step 1: Forward declarations (classes).
${CLASS_PATH_DECLARATIONS}

// Step 2: Forward declarations (methods).

${FORWARD_DECLARATIONS}

// Step 3: Method declarations.

${JNI_NATIVE_METHOD_ARRAY}\
${PROXY_NATIVE_METHOD_ARRAY}\

${JNI_NATIVE_METHOD}
// Step 4: Main dex and non-main dex registration functions.

namespace ${NAMESPACE} {

bool RegisterMainDexNatives(JNIEnv* env) {\
${REGISTER_MAIN_DEX_PROXY_NATIVES}
${REGISTER_MAIN_DEX_NATIVES}
  return true;
}

bool RegisterNonMainDexNatives(JNIEnv* env) {\
${REGISTER_PROXY_NATIVES}
${REGISTER_NON_MAIN_DEX_NATIVES}
  return true;
}

}  // namespace ${NAMESPACE}

#endif  // ${HEADER_GUARD}
""")
  _SetProxyRegistrationFields(registration_dict)

  if len(registration_dict['FORWARD_DECLARATIONS']) == 0:
    return ''

  return template.substitute(registration_dict)


class HeaderGenerator(object):
  """Generates an inline header file for JNI registration."""

  def __init__(self, namespace, fully_qualified_class, natives, jni_params,
               main_dex):
    self.namespace = namespace
    self.natives = natives
    self.proxy_natives = [n for n in natives if n.is_proxy]
    self.non_proxy_natives = [n for n in natives if not n.is_proxy]
    self.fully_qualified_class = fully_qualified_class
    self.jni_params = jni_params
    self.class_name = self.fully_qualified_class.split('/')[-1]
    self.main_dex = main_dex
    self.helper = jni_generator.HeaderFileGeneratorHelper(
        self.class_name, fully_qualified_class)
    self.registration_dict = None

  def Generate(self):
    self.registration_dict = {'FULL_CLASS_NAME': self.fully_qualified_class}
    self._AddClassPathDeclarations()
    self._AddForwardDeclaration()
    self._AddJNINativeMethodsArrays()
    self._AddProxyNativeMethodKStrings()
    self._AddRegisterNativesCalls()
    self._AddRegisterNativesFunctions()
    return self.registration_dict

  def _SetDictValue(self, key, value):
    self.registration_dict[key] = jni_generator.WrapOutput(value)

  def _AddClassPathDeclarations(self):
    classes = self.helper.GetUniqueClasses(self.natives)
    self._SetDictValue('CLASS_PATH_DECLARATIONS',
        self.helper.GetClassPathLines(classes, declare_only=True))

  def _AddForwardDeclaration(self):
    """Add the content of the forward declaration to the dictionary."""
    template = string.Template("""\
JNI_GENERATOR_EXPORT ${RETURN} ${STUB_NAME}(
    JNIEnv* env,
    ${PARAMS_IN_STUB});
""")
    forward_declaration = ''
    for native in self.natives:
      value = {
          'RETURN': jni_generator.JavaDataTypeToC(native.return_type),
          'STUB_NAME': self.helper.GetStubName(native),
          'PARAMS_IN_STUB': jni_generator.GetParamsInStub(native),
      }
      forward_declaration += template.substitute(value)
    self._SetDictValue('FORWARD_DECLARATIONS', forward_declaration)

  def _AddRegisterNativesCalls(self):
    """Add the body of the RegisterNativesImpl method to the dictionary."""

    # Only register if there is at least 1 non-proxy native
    if len(self.non_proxy_natives) == 0:
      return ''

    template = string.Template("""\
  if (!${REGISTER_NAME}(env))
    return false;
""")
    value = {
        'REGISTER_NAME':
            jni_generator.GetRegistrationFunctionName(
                self.fully_qualified_class)
    }
    register_body = template.substitute(value)
    if self.main_dex:
      self._SetDictValue('REGISTER_MAIN_DEX_NATIVES', register_body)
    else:
      self._SetDictValue('REGISTER_NON_MAIN_DEX_NATIVES', register_body)

  def _AddJNINativeMethodsArrays(self):
    """Returns the implementation of the array of native methods."""
    template = string.Template("""\
static const JNINativeMethod kMethods_${JAVA_CLASS}[] = {
${KMETHODS}
};

""")
    open_namespace = ''
    close_namespace = ''
    if self.namespace:
      parts = self.namespace.split('::')
      all_namespaces = ['namespace %s {' % ns for ns in parts]
      open_namespace = '\n'.join(all_namespaces) + '\n'
      all_namespaces = ['}  // namespace %s' % ns for ns in parts]
      all_namespaces.reverse()
      close_namespace = '\n'.join(all_namespaces) + '\n\n'

    body = self._SubstituteNativeMethods(template)
    self._SetDictValue('JNI_NATIVE_METHOD_ARRAY',
                       ''.join((open_namespace, body, close_namespace)))

  def _GetKMethodsString(self, clazz):
    ret = []
    for native in self.non_proxy_natives:
      if (native.java_class_name == clazz or
          (not native.java_class_name and clazz == self.class_name)):
        ret += [self._GetKMethodArrayEntry(native)]
    return '\n'.join(ret)

  def _GetKMethodArrayEntry(self, native):
    template = string.Template('    { "${NAME}", ${JNI_SIGNATURE}, ' +
                               'reinterpret_cast<void*>(${STUB_NAME}) },')

    name = 'native' + native.name
    if native.is_proxy:
      # Literal name of the native method in the class that contains the actual
      # native declaration.
      name = native.proxy_name
    values = {
        'NAME':
            name,
        'JNI_SIGNATURE':
            self.jni_params.Signature(native.params, native.return_type),
        'STUB_NAME':
            self.helper.GetStubName(native)
    }
    return template.substitute(values)

  def _AddProxyNativeMethodKStrings(self):
    """Returns KMethodString for wrapped native methods in all_classes """

    if self.main_dex:
      key = 'PROXY_NATIVE_METHOD_ARRAY_MAIN_DEX'
    else:
      key = 'PROXY_NATIVE_METHOD_ARRAY'

    proxy_k_strings = ('\n'.join(
        [self._GetKMethodArrayEntry(p) for p in self.proxy_natives]))

    self._SetDictValue(key, proxy_k_strings)

  def _SubstituteNativeMethods(self, template, sub_proxy=False):
    """Substitutes NAMESPACE, JAVA_CLASS and KMETHODS in the provided
    template."""
    ret = []
    all_classes = self.helper.GetUniqueClasses(self.natives)
    all_classes[self.class_name] = self.fully_qualified_class

    for clazz, full_clazz in all_classes.iteritems():
      if not sub_proxy:
        if clazz == jni_generator.NATIVE_PROXY_CLASS_NAME:
          continue

      kmethods = self._GetKMethodsString(clazz)
      namespace_str = ''
      if self.namespace:
        namespace_str = self.namespace + '::'
      if kmethods:
        values = {
            'NAMESPACE': namespace_str,
            'JAVA_CLASS': jni_generator.EscapeClassName(full_clazz),
            'KMETHODS': kmethods
        }
        ret += [template.substitute(values)]
    if not ret: return ''
    return '\n'.join(ret)

  def GetJNINativeMethodsString(self):
    """Returns the implementation of the array of native methods."""
    template = string.Template("""\
static const JNINativeMethod kMethods_${JAVA_CLASS}[] = {
${KMETHODS}

};
""")
    return self._SubstituteNativeMethods(template)

  def _AddRegisterNativesFunctions(self):
    """Returns the code for RegisterNatives."""
    natives = self._GetRegisterNativesImplString()
    if not natives:
      return ''
    template = string.Template("""\
JNI_REGISTRATION_EXPORT bool ${REGISTER_NAME}(JNIEnv* env) {
${NATIVES}\
  return true;
}

""")
    values = {
      'REGISTER_NAME': jni_generator.GetRegistrationFunctionName(
          self.fully_qualified_class),
      'NATIVES': natives
    }
    self._SetDictValue('JNI_NATIVE_METHOD', template.substitute(values))

  def _GetRegisterNativesImplString(self):
    """Returns the shared implementation for RegisterNatives."""
    template = string.Template("""\
  const int kMethods_${JAVA_CLASS}Size =
      arraysize(${NAMESPACE}kMethods_${JAVA_CLASS});
  if (env->RegisterNatives(
      ${JAVA_CLASS}_clazz(env),
      ${NAMESPACE}kMethods_${JAVA_CLASS},
      kMethods_${JAVA_CLASS}Size) < 0) {
    jni_generator::HandleRegistrationError(env,
        ${JAVA_CLASS}_clazz(env),
        __FILE__);
    return false;
  }

""")
    # Only register if there is a native method not in a proxy,
    # since all the proxies will be registered together.
    if len(self.non_proxy_natives) != 0:
      return self._SubstituteNativeMethods(template)
    return ''


def main(argv):
  arg_parser = argparse.ArgumentParser()
  build_utils.AddDepfileOption(arg_parser)

  arg_parser.add_argument('--sources_files',
                          help='A list of .sources files which contain Java '
                          'file paths. Must be used with --output.')
  arg_parser.add_argument('--output',
                          help='The output file path.')
  arg_parser.add_argument('--no_register_java',
                          default=[],
                          help='A list of Java files which should be ignored '
                          'by the parser.')
  arg_parser.add_argument('--namespace',
                          default='',
                          help='Namespace to wrap the registration functions '
                          'into.')
  args = arg_parser.parse_args(build_utils.ExpandFileArgs(argv[1:]))
  args.sources_files = build_utils.ParseGnList(args.sources_files)

  if not args.sources_files:
    print '\nError: Must specify --sources_files.'
    return 1

  java_file_paths = []
  for f in args.sources_files:
    # java_file_paths stores each Java file path as a string.
    java_file_paths += build_utils.ReadSourcesList(f)
  output_file = args.output
  GenerateJNIHeader(java_file_paths, output_file, args)

  if args.depfile:
    build_utils.WriteDepfile(args.depfile, output_file,
                             args.sources_files + java_file_paths,
                             add_pydeps=False)


if __name__ == '__main__':
  sys.exit(main(sys.argv))
