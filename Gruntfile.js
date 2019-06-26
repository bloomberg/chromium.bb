module.exports = function(grunt) {
  // Project configuration.
  grunt.initConfig({
    pkg: grunt.file.readJSON('package.json'),

    clean: ['out/'],

    mkdir: {
      out: {
        options: {
          create: ['out'],
        },
      },
    },

    run: {
      'generate-listings': {
        cmd: 'tools/gen',
        args: ['cts', 'unittests', 'demos'],
      },
      'generate-version': {
        cmd: 'tools/gen_version',
      },
      test: {
        cmd: 'tools/run',
        args: ['unittests'],
      },
      'build-out': {
        cmd: 'node_modules/.bin/babel',
        args: ['--source-maps', 'true', '--extensions', '.ts', '--out-dir', 'out/', 'src/'],
      },
      'build-shaderc': {
        cmd: 'node_modules/.bin/babel',
        args: [
          '--plugins',
          'babel-plugin-transform-commonjs-es2015-modules',
          'node_modules/@webgpu/shaderc/dist/index.js',
          '-o',
          'out/shaderc.js',
        ],
      },
      'gts-check': { cmd: 'node_modules/.bin/gts', args: ['check'] },
      'gts-fix': { cmd: 'node_modules/.bin/gts', args: ['fix'] },
    },

    'http-server': {
      '.': {
        root: '.',
        port: 8080,
        host: '127.0.0.1',
        cache: 5,
      },
    },

    ts: {
      check: {
        tsconfig: {
          tsconfig: 'tsconfig.json',
          passThrough: true,
        },
      },
    },
  });

  grunt.loadNpmTasks('grunt-contrib-clean');
  grunt.loadNpmTasks('grunt-http-server');
  grunt.loadNpmTasks('grunt-mkdir');
  grunt.loadNpmTasks('grunt-run');
  grunt.loadNpmTasks('grunt-ts');

  const publishedTasks = [];
  function publishTask(name, desc, deps) {
    grunt.registerTask(name, deps);
    publishedTasks.push({ name, desc });
  }

  publishTask('check', 'Check types and styles', ['ts:check', 'run:gts-check']);
  publishTask('fix', 'Fix lint and formatting', ['run:gts-fix']);
  publishTask('build', 'Build out/', [
    'mkdir:out',
    'run:build-shaderc',
    'run:build-out',
    'run:generate-version',
    'run:generate-listings',
  ]);
  publishTask('test', 'Run unittests', ['run:test']);
  publishTask('serve', 'Serve out/ on 127.0.0.1:8080', ['http-server:.']);
  publishedTasks.push({ name: 'clean', desc: 'Clean out/' });

  grunt.registerTask('default', '', () => {
    console.log('Available tasks (see grunt --help for info):');
    for (const { name, desc } of publishedTasks) {
      console.log(`$ grunt ${name}`);
      console.log(`  ${desc}`);
    }
  });

  publishTask('pre', 'Run all presubmit checks', ['ts:check', 'build', 'test', 'run:gts-check']);
};
