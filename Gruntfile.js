module.exports = function(grunt) {
  const mkRun = (inspect, ...args) => {
    return {
      cmd: 'node',
      args: [
        ...(inspect ? ['--inspect-brk'] : []),
        '-r', 'ts-node/register/transpile-only',
        'tools/run.js',
        ...args
      ]
    };
  };

  const mkGen = (inspect, ...args) => {
    return {
      cmd: 'node',
      args: [
        ...(inspect ? ['--inspect-brk'] : []),
        '-r', 'ts-node/register/transpile-only',
        'tools/gen.js',
        ...args
      ]
    };
  };

  // Project configuration.
  grunt.initConfig({
    pkg: grunt.file.readJSON('package.json'),

    clean: ['out/'],

    mkdir: {
      'out': {
        options: {
          create: ['out']
        }
      }
    },

    run: {
      'generate-listings': mkGen(false, 'cts', 'unittests', 'demos'),
      'cts': mkRun(false, 'cts'),
      'debug-cts': mkRun(true, 'cts'),
      'unittests': mkRun(false, 'unittests'),
      'debug-unittests': mkRun(true, 'unittests'),
      'demos': mkRun(false, 'demos'),
      'debug-demos': mkRun(true, 'demos'),
      'build-out': {
        cmd: 'npx',
        args: [
          'babel',
          '--source-maps', 'true',
          '--extensions', '.ts',
          '--out-dir', 'out/',
          'src/',
        ]
      },
      'build-shaderc': {
        cmd: 'npx',
        args: [
          'babel',
          '--plugins', 'babel-plugin-transform-commonjs-es2015-modules',
          'node_modules/@webgpu/shaderc/dist/index.js',
          '-o', 'out/shaderc.js',
        ]
      },
      'lint': {
        cmd: 'npx',
        args: ['tslint', '--project', '.']
      },
      'fix': {
        cmd: 'npx',
        args: ['tslint', '--project', '.', '--fix']
      },
      'check-git-is-clean': {
        cmd: 'git',
        args: ['diff-index', '--quiet', 'HEAD', '--']
      }
    },

    'http-server': {
      '.': {
        root: '.',
        port: 8080,
        host: '127.0.0.1',
        cache: 5,
      }
    },

    ts: {
      'check': {
        tsconfig: {
          tsconfig: 'tsconfig.json',
          passThrough: true,
        },
      },

      'out/': {
        tsconfig: {
          tsconfig: 'tsconfig.web.json',
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

  publishTask('check', 'Check Typescript build', [
    'ts:check',
  ]);
  publishedTasks.push({ name: 'run:{lint,fix}', desc: 'Run tslint' });
  publishTask('build', 'Build out/', [
    'mkdir:out',
    'run:build-shaderc',
    'run:build-out',
    'run:generate-listings',
  ]);
  publishTask('serve', 'Serve out/ on 127.0.0.1:8080', [
    'http-server:.',
  ]);
  publishedTasks.push({ name: 'clean', desc: 'Clean out/' });

  publishedTasks.push({ name: 'run:{cts,unittests,demos}', desc: '(Node) Run {cts,unittests,demos}' });
  publishedTasks.push({ name: 'run:debug-{cts,unittests,demos}', desc: '(Node) Debug {cts,unittests,demos}' });

  grunt.registerTask('default', '', () => {
    console.log('Available tasks (see grunt --help for info):');
    for (const { name, desc } of publishedTasks) {
      console.log(`$ grunt ${name}`);
      console.log(`  ${desc}`);
    }
  });

  publishTask('presubmit', 'Run all presubmit checks', [
    'check',
    'build',
    'run:unittests',

    // 'format',  // TODO
    'run:fix',
    'run:check-git-is-clean',
  ]);
};
