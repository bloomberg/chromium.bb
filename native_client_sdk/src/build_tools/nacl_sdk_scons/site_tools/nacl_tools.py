# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Extend the SCons Environment object with NaCl-specific builders and
modifiers.
'''

from SCons import Script

import nacl_utils
import os
import SCons

def FilterOut(env, **kw):
  """Removes values from existing construction variables in an Environment.

  The values to remove should be a list.  For example:

  env.FilterOut(CPPDEFINES=['REMOVE_ME', 'ME_TOO'])

  Args:
    env: Environment to alter.
    kw: (Any other named arguments are values to remove).
  """

  kw = SCons.Environment.copy_non_reserved_keywords(kw)

  for key, val in kw.items():
    if key in env:
      # Filter out the specified values without modifying the original list.
      # This helps isolate us if a list is accidently shared
      # NOTE if env[key] is a UserList, this changes the type into a plain
      # list.  This is OK because SCons also does this in semi_deepcopy
      env[key] = [item for item in env[key] if item not in val]

    # TODO: SCons.Environment.Append() has much more logic to deal with various
    # types of values.  We should handle all those cases in here too.  (If
    # variable is a dict, etc.)

def AppendOptCCFlags(env, is_debug=False):
  '''Append a set of CCFLAGS that will build a debug or optimized variant
  depending on the value of |is_debug|.

  Uses optional build-specific flags for debug and optimized builds.  To set
  these in your build.scons files you can do something like this:
    nacl_env.Append(DEBUG_CCFLAGS=['-gfull'],
                    OPT_CCFLAGS=['-ffast-math',
                                 '-mfpmath=sse',
                                 '-msse2'])

  Args:
    env: Environment to modify.
    is_debug: Whether to set the option flags for debugging or not.  Default
      value is False.
  '''

  if is_debug:
    env.Append(CCFLAGS=['${DEBUG_CCFLAGS}',
                        '-O0',
                        '-g',
                       ])
  else:
    env.Append(CCFLAGS=['${OPT_CCFLAGS}',
                        '-O3',
                        '-fno-stack-protector',
                        '-fomit-frame-pointer',
                       ])


def AppendArchFlags(env, arch_spec):
  '''Append a set of architecture-specific flags to the environment.

  |arch_spec| is expected to be a map containing the keys "arch" and "subarch".
  Supported keys are:
      arch: x86
      subarch: 32 | 64

  Args:
    env: Environment to modify.
    arch_spec: A dictionary with keys describing the arch and subarch to build.
      Possible values are: 'arch': x86; 'subarch': 32 or 64.
  '''

  arch, subarch = nacl_utils.GetArchFromSpec(arch_spec)
  cc_arch_flags = ['-m%s' % subarch]
  as_arch_flags = ['--%s' % subarch]
  if subarch == '64':
    ld_arch_flags = ['-m64']
    env['NACL_ARCHITECTURE'] = 'x86_64-nacl-'
  else:
    ld_arch_flags = ['-m32']
    env['NACL_ARCHITECTURE'] = 'i686-nacl-'
  env.Append(ASFLAGS=as_arch_flags,
             CCFLAGS=cc_arch_flags,
             LINKFLAGS=ld_arch_flags)


def NaClProgram(env, target, sources, variant_dir='obj'):
  '''Add a Program to env that builds its objects in the directory specified
  by |variant_dir|.

  This is slightly different than VariantDir() in that the sources can live in
  the same directory as the calling SConscript file.

  Args:
    env: Environment to modify.
    target: The target name that depends on the object files.  E.g.
      "hello_world_x86_32.nexe"
    sources: The list of source files that are used to build the objects.
    variant_dir: The built object files are put in this directory.  Default
      value is "obj".

  Returns:
    The Program Node.
  '''

  program_objects = []
  for src_file in sources:
    obj_file = os.path.splitext(src_file)[0] + env.get('OBJSUFFIX', '.o')
    program_objects.append(env.StaticObject(
        target=os.path.join(variant_dir, obj_file), source=src_file))
  env.Clean('.', variant_dir)
  return env.Program(target, program_objects)


def NaClTestProgram(env,
                    test_sources,
                    arch_spec,
                    module_name='nacl_test',
                    target_name='test'):
  '''Modify |env| to include an Alias node for a test named |test_name|.

  This node will build the desired NaCl module with the debug flags turned on.
  The Alias node has a build action that runs the test under sel_ldr.  |env| is
  expected to have variables named 'NACL_SEL_LDR<x>', and 'NACL_IRT_CORE<x>'
  where <x> is the various architectures supported (e.g. NACL_SEL_LDR32 and
  NACL_SEL_LLDR64)

  Args:
    env: Environment to modify.
    test_sources: The list of source files that are used to build the objects.
    arch_spec: A dictionary with keys describing the arch and subarch to build.
      Possible values are: 'arch': x86; 'subarch': 32 or 64.
    module_name: The name of the module.  The name of the output program
        incorporates this as its prefix.
    target_name: The name of the final Alias node.  This name can be given on
        the command line.  For example:
            nacl_env.NaClTestProgram(nacl_utils.ARCH_SPECS['x86-32'],
                                     'hello_world_test',
                                     'test32')
        will let you say: ./scons test32 to build and run hello_world_test.
  Returns:
    A list of Nodes, one for each architecture-specific test.
  '''

  arch, subarch = nacl_utils.GetArchFromSpec(arch_spec)
  # Create multi-level dictionary for sel_ldr binary name.
  NACL_SEL_LDR = {'x86' :
                   {'32': '$NACL_SEL_LDR32',
                    '64': '$NACL_SEL_LDR64'
                   }
                 }
  NACL_IRT_CORE = {'x86' :
                    {'32': '$NACL_IRT_CORE32',
                     '64': '$NACL_IRT_CORE64'
                    }
                  }
  arch_sel_ldr = NACL_SEL_LDR[arch][subarch]
  # if |arch| and |subarch| are not found, a KeyError exception will be
  # thrown, which will generate a stack trace for debugging.
  test_program = nacl_utils.MakeNaClModuleEnvironment(
                     env,
                     test_sources,
                     module_name,
                     arch_spec,
                     is_debug=True,
                     build_dir_prefix='test_')
  test_node = env.Alias(target_name,
                        source=test_program,
                        action=arch_sel_ldr +
                               ' -B %s' % NACL_IRT_CORE[arch][subarch] +
                               ' $SOURCE')
  # Tell SCons that |test_node| never goes out of date, so that you don't see
  # '<test_node> is up to date.'
  env.AlwaysBuild(test_node)


def NaClStaticLib(env, target, sources, variant_dir='obj',
                  lib_name='', lib_dir=''):
  '''Add a StaticLibrary to env that builds its objects in the directory
  specified by |variant_dir|.

  This is slightly different than VariantDir() in that the sources can live in
  the same directory as the calling SConscript file.

  Args:
    env: Environment to modify.
    target: The target name that depends on the object files.  E.g.
      "c_salt_x86_32" yields the target name libc_salt_x86_32.a.
    sources: The list of source files that are used to build the objects.
    variant_dir: The built object files are put in this directory.  Default
      value is "obj".
    lib_name: The final name for the library. E.g. lib_name='c_salt' yields
      the final library name libc_)salt.a. Defaults to the target name.
    lib_dir: The final library file is placed in that directory. The directory
      is created if it doesn't already exists. Default is '.'.

  Returns:
    The StaticLibrary Node.
  '''

  program_objects = []
  for src_file in sources:
    obj_file = os.path.splitext(src_file)[0] + env.get('OBJSUFFIX', '.o')
    program_objects.append(env.StaticObject(
        target=os.path.join(variant_dir, obj_file), source=src_file))
  env.Clean('.', variant_dir)
  lib_file = env.StaticLibrary(target, program_objects)

  # If either a lib_name or lib_dir, we must move the library file to its
  # final destination.
  if lib_dir or lib_name:
    # Map lib_name to an actual file name that includes the correct prefix and
    # suffix, both extracted from the target library name generated by scons.
    if lib_name:
      final_lib_name = lib_file[0].name.replace(target, lib_name)
    else:
      final_lib_name = lib_file[0].name
    # Add an action to move the file.
    install_node = env.InstallAs(os.path.join(lib_dir, final_lib_name),
                                 lib_file)
    # Add lib_name as an alias. This is the target that will build and install
    # the libraries.
    if lib_name:
      env.Alias(lib_name, install_node)

  return lib_file


def NaClStrippedInstall(env, dir='', source=None):
  '''Strip the target node.

  Args:
    env: Environment to modify.
    dir: The root install directory.
    source: a list of a list of Nodes representing the executables to be
        stripped.

  Returns:
    A list of Install Nodes that strip each buildable in |source|.
  '''
  stripped_install_nodes = []
  if not source:
    return stripped_install_nodes
  for source_nodes in source:
    for strip_node in source_nodes:
      # Use the construction Environment used to create the buildable object.
      # Each environment has various important properties, such as the target
      # architecture and tools prefix.
      strip_env = strip_node.get_env()
      install_node = strip_env.Install(dir=strip_env['NACL_INSTALL_ROOT'],
                                       source=strip_node)
      strip_env.AddPostAction(install_node, "%s $TARGET" % strip_env['STRIP'])
      stripped_install_nodes.append(install_node)

  return stripped_install_nodes


def NaClModules(env, sources, module_name, is_debug=False):
  '''Produce one construction Environment for each supported instruction set
  architecture.

  Args:
    env: Environment to modify.
    sources: The list of source files that are used to build the objects.
    module_name: The name of the module.
    is_debug: Whether to set the option flags for debugging or not.  Default
      value is False.

  Returns:
    A list of SCons build Nodes, each one with settings specific to an
        instruction set architecture.
  '''
  return [
      nacl_utils.MakeNaClModuleEnvironment(
          env,
          sources,
          module_name=module_name,
          arch_spec=nacl_utils.ARCH_SPECS['x86-32'],
          is_debug=is_debug),
      nacl_utils.MakeNaClModuleEnvironment(
          env,
          sources,
          module_name=module_name,
          arch_spec=nacl_utils.ARCH_SPECS['x86-64'],
          is_debug=is_debug),
  ]


def NaClStaticLibraries(env, sources, lib_name, is_debug=False, lib_dir=''):
  '''Produce one static-lib construction Environment for each supported
  instruction set architecture.

  Args:
    env: Environment to modify.
    sources: The list of source files that are used to build the objects.
    lib_name: The name of the static lib.
    is_debug: Whether to set the option flags for debugging or not.  Default
      value is False.
    lib_dir: Where to output the final library file. Lib files are placed in
        directories within lib_dir, based on the arch type and build type.
        E.g. Specifying lib_dir = 'dest' for lib name 'c_salt' on x86-32 debug
        creates the library file 'dest/lib-x86-32/dbg/libc_salt.a'

  Returns:
    A list of SCons build Nodes, each one with settings specific to an
        instruction set architecture.
  '''
  return [
      nacl_utils.MakeNaClStaticLibEnvironment(
          env,
          sources,
          lib_name=lib_name,
          arch_spec=nacl_utils.ARCH_SPECS['x86-32'],
          is_debug=is_debug,
          lib_dir=lib_dir),
      nacl_utils.MakeNaClStaticLibEnvironment(
          env,
          sources,
          lib_name=lib_name,
          arch_spec=nacl_utils.ARCH_SPECS['x86-64'],
          is_debug=is_debug,
          lib_dir=lib_dir),
  ]


def InstallPrebuilt(env, module_name):
  '''Create the 'install_prebuilt' target.

  install_prebuilt is used by the SDK build machinery to provide a prebuilt
  version of the example in the SDK installer.  This pseudo-builder adds an
  Alias node called 'install_prebuilt' that depends on the main .nmf file of
  the example.  The .nmf file in turn has all the right dependencies to build
  the necessary NaCl modules.  As a final step, the opt variant directories
  are removed.  Once this build is done, the SDK builder can include the
  example directory in its installer.

  Args:
    env: Environment to modify.
    module_name: The name of the module.

  Returns:
    The Alias node representing the install_prebuilt target.
  '''

  return env.Alias('install_prebuilt',
                   source=['%s.nmf' % module_name],
                   action=Script.Delete([env.Dir('opt_x86_32'),
                                         env.Dir('opt_x86_64')]))


def AllNaClModules(env, sources, module_name):
  '''Add a builder for both the debug and optimized variant of every supported
  instruction set architecture.

  Add one builder for each variant of the NaCl module, and also generate the
  .nmf that loads the resulting NaCl modules.  The .nmf file is named
  |module_name|.nmf; similarly all other build products have |module_name| in
  their name, e.g.
      nacl_env.AllNaClModules(sources, module_name='hello_world')
  produces these files, when x86-64 and x86-32 architectures are supported:
      hello_world.nmf
      hello_world_dbg.nmf
      hello_world_x86_32.nexe
      hello_world_x86_64.nexe
      hello_world_x86_32_dbg.nexe
      hello_world_x86_64_dbg.nexe
  Object files go in variant directories named 'dbg_*' for debug builds and
  'opt_*' for optimized builds, where the * is a string describing the
  architecture, e.g. 'x86_32'.

  Args:
    env: Environment to modify.
    sources: The list of source files that are used to build the objects.
    module_name: The name of the module.

  Returns:
    A 2-tuple of SCons Program nodes, the first element is the node that
        builds optimized .nexes; the second builds the debug .nexes.
  '''

  opt_nexes = env.NaClModules(sources, module_name, is_debug=False)
  env.GenerateNmf(target='%s.nmf' % module_name,
                  source=opt_nexes,
                  nexes={'x86-32': '%s_x86_32.nexe' % module_name,
                         'x86-64': '%s_x86_64.nexe' % module_name})

  dbg_nexes = env.NaClModules(sources, module_name, is_debug=True)
  env.GenerateNmf(target='%s_dbg.nmf' % module_name,
                  source=dbg_nexes,
                  nexes={'x86-32': '%s_x86_32_dbg.nexe' % module_name,
                         'x86-64': '%s_x86_64_dbg.nexe' % module_name})
  nacl_utils.PrintNaclPlatformBanner(module_name,
      nacl_platform=env['TARGET_NACL_PLATFORM'],
      variant=env['NACL_TOOLCHAIN_VARIANT'])

  return opt_nexes, dbg_nexes


def AllNaClStaticLibraries(env, sources, lib_name, lib_dir=''):
  '''Add a builder for both the debug and optimized variant of every supported
  instruction set architecture.

  Add one builder for each variant of the NaCl static library. The library
  names have |lib_name| in their name, e.g.
      nacl_env.AllNaClModules(sources, lib_name='c_salt')
  produces these files, when x86-64 and x86-32 architectures are supported:
      libc_salt_x86_32.a
      libc_salt_x86_64.a
  Object files go in variant directories named 'dbg_*' for debug builds and
  'opt_*' for optimized builds, where the * is a string describing the
  architecture, e.g. 'x86_32'.

  Args:
    env: Environment to modify.
    sources: The list of source files that are used to build the objects.
    lib_name: The name of the static library.
    lib_dir: Where to output the final library files. Lib files are placed in
        directories within lib_dir, based on the arch type and build type.
        E.g. Specifying lib_dir = 'dest' for lib name 'c_salt' on x-86-32 debug
        creates the library file 'dest/lib-x86-32/dbg/libc_salt.a'

  Returns:
    A 2-tuple of SCons StaticLibrary nodes, the first element is the node that
        builds optimized libs; the second builds the debug libs.
  '''

  opt_libs = env.NaClStaticLibraries(sources, lib_name, is_debug=False,
                                     lib_dir=lib_dir)
  dbg_libs = env.NaClStaticLibraries(sources, lib_name, is_debug=True,
                                     lib_dir=lib_dir)
  nacl_utils.PrintNaclPlatformBanner(lib_name,
      nacl_platform=env['TARGET_NACL_PLATFORM'],
      variant=env['NACL_TOOLCHAIN_VARIANT'])

  return opt_libs, dbg_libs


def generate(env):
  '''SCons entry point for this tool.

  Args:
    env: The SCons Environment to modify.

  NOTE: SCons requires the use of this name, which fails lint.
  '''
  nacl_utils.AddCommandLineOptions()

  env.AddMethod(AllNaClModules)
  env.AddMethod(AllNaClStaticLibraries)
  env.AddMethod(AppendOptCCFlags)
  env.AddMethod(AppendArchFlags)
  env.AddMethod(FilterOut)
  env.AddMethod(InstallPrebuilt)
  env.AddMethod(NaClProgram)
  env.AddMethod(NaClTestProgram)
  env.AddMethod(NaClStaticLib)
  env.AddMethod(NaClModules)
  env.AddMethod(NaClStaticLibraries)
  env.AddMethod(NaClStrippedInstall)


def exists(env):
  '''The NaCl tool is always valid.  This is a required entry point for SCons
    Tools.

  Args:
    env: The SCons Environment this tool will run in.

  Returns:
    Always returns True.
  '''
  return True
