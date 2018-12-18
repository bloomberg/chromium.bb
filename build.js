const fs = require('fs');

const browserify = require('browserify');
const tsify = require('tsify');

if (!fs.existsSync('out/')) {
  fs.mkdirSync('out/');
}

browserify({
  debug: true,
  paths: ['src'],
})
  .add('src/index.ts')
  .plugin(tsify)
  .bundle()
  .on('error', error => { console.error(error.toString()) })
  .pipe(fs.createWriteStream('out/main.js'));
